// SPDX-License-Identifier: GPL-2.0+
/*
 * LUKS (Linux Unified Key Setup) command
 *
 * Copyright (C) 2025 Canonical Ltd
 */

#include <blk.h>
#include <command.h>
#include <dm.h>
#include <getopt.h>
#include <hexdump.h>
#include <luks.h>
#include <part.h>
#include <tkey.h>
#include <u-boot/sha256.h>

/* messages which are too long to sit in the call which reports them */
#define UNLOCK_ERR	"Failed to unlock LUKS partition (err %dE)\n"
#define BLKMAP_ERR	"Failed to create blkmap device (err %dE)\n"

static int do_luks_detect(struct getopt_state *gs)
{
	struct blk_desc *dev_desc;
	struct disk_partition info;
	int part, ret, version;

	if (gs->argc != 3)
		return CMD_RET_USAGE;

	part = blk_get_device_part_str(gs->argv[1], gs->argv[2], &dev_desc,
				       &info, 1);
	if (part < 0)
		return CMD_RET_FAILURE;

	ret = luks_detect(dev_desc->bdev, &info);
	if (ret < 0)
		return cmd_fail(ret, "Not a LUKS partition (error %dE)\n", ret);
	version = luks_get_version(dev_desc->bdev, &info);
	printf("LUKS%d encrypted partition detected\n", version);

	return CMD_RET_SUCCESS;
}

static int do_luks_info(struct getopt_state *gs)
{
	struct blk_desc *dev_desc;
	struct disk_partition info;
	int part, ret;

	if (gs->argc != 3)
		return CMD_RET_USAGE;

	part = blk_get_device_part_str(gs->argv[1], gs->argv[2], &dev_desc,
				       &info, 1);
	if (part < 0)
		return CMD_RET_FAILURE;

	ret = luks_show_info(dev_desc->bdev, &info);
	if (ret < 0)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

/**
 * unlock_with_tkey() - Unlock LUKS partition using TKey-derived key
 *
 * This function uses TKey to derive a disk encryption key from the
 * provided passphrase (used as USS) and uses it to unlock the LUKS partition.
 *
 * @dev_desc:	Block device descriptor
 * @info:	Partition information
 * @passphrase:	Passphrase to use as USS for TKey
 * @master_key:	Buffer to receive unlocked master key
 * @key_size:	Pointer to receive key size
 * Return: 0 on success, -ve on error
 */
static int unlock_with_tkey(struct blk_desc *dev_desc,
			    struct disk_partition *info, const char *passphrase,
			    u8 *master_key, u32 *key_size)
{
	u8 tkey_disk_key[TKEY_DISK_KEY_SIZE];
	u8 pubkey[TKEY_PUBKEY_SIZE];
	struct udevice *tkey_dev;
	int ret;

	printf("Using TKey for disk encryption key\n");

	/* Find TKey device */
	tkey_dev = tkey_get_device();
	if (!tkey_dev) {
		cmd_err("Failed to find TKey device\n");
		return -ENOENT;
	}

	/* Derive disk key using TKey with passphrase as USS */
	printf("Loading TKey signer app (%lu bytes) with USS...\n",
	       (ulong)TKEY_SIGNER_SIZE);
	ret = tkey_derive_disk_key(tkey_dev, (const u8 *)__signer_1_0_0_begin,
				   TKEY_SIGNER_SIZE, (const u8 *)passphrase,
				   strlen(passphrase), tkey_disk_key, pubkey,
				   NULL);
	if (ret) {
		cmd_err("Failed to derive TKey disk key (err %dE)\n", ret);
		return ret;
	}

	printf("TKey public key: ");
	print_hex_dump("  ", DUMP_PREFIX_NONE, 16, 1, pubkey,
		       TKEY_PUBKEY_SIZE, false);

	printf("TKey disk key derived successfully\n");
	printf("TKey derived disk key: ");
	print_hex_dump("  ", DUMP_PREFIX_NONE, 16, 1, tkey_disk_key,
		       TKEY_DISK_KEY_SIZE, false);

	ret = luks_unlock(dev_desc->bdev, info, tkey_disk_key,
			  TKEY_DISK_KEY_SIZE, false, master_key, key_size);

	/* Wipe TKey disk key */
	memset(tkey_disk_key, '\0', sizeof(tkey_disk_key));

	return ret;
}

static int do_luks_unlock(struct getopt_state *gs)
{
	struct blk_desc *dev_desc;
	struct disk_partition info;
	struct udevice *blkmap_dev;
	const char *iface, *devpart;
	const char *passphrase = NULL;
	bool use_tkey = false;
	bool pre_derived = false;
	int part, ret, version, opt;
	u8 master_key[128];
	char label[64];
	u32 key_size;

	while ((opt = getopt(gs, "+tp")) > 0) {
		switch (opt) {
		case 't':
			use_tkey = true;
			break;
		case 'p':
			pre_derived = true;
			break;
		default:
			return CMD_RET_USAGE;
		}
	}

	/*
	 * Each option says where the key comes from, so the two are
	 * alternatives: -t wins if both are given
	 */
	if (IS_ENABLED(CONFIG_CMD_ARG_CHECK) && use_tkey && pre_derived)
		return CMD_RET_USAGE;

	if (gs->argc - gs->index != 3)
		return CMD_RET_USAGE;

	iface = getopt_pop(gs);
	devpart = getopt_pop(gs);

	part = blk_get_device_part_str(iface, devpart, &dev_desc, &info, 1);
	if (part < 0)
		return CMD_RET_FAILURE;

	passphrase = getopt_pop(gs);

	log_debug("Partition start %llx blks %llx blksz%lx\n",
		  (unsigned long long)info.start, (unsigned long long)info.size,
		  (ulong)dev_desc->blksz);

	/* Verify it's a LUKS partition */
	version = luks_get_version(dev_desc->bdev, &info);
	if (version < 0)
		return cmd_fail(version, "Not a LUKS partition\n");

	printf("Unlocking LUKS%d partition...\n", version);

	if (use_tkey) {
		ret = unlock_with_tkey(dev_desc, &info, passphrase, master_key,
				       &key_size);
	} else if (pre_derived) {
		/* Pre-derived key: passphrase is hex-encoded master key */
		u8 key_buf[64];
		size_t key_len = strlen(passphrase) / 2;

		if (key_len > sizeof(key_buf) || hex2bin(key_buf, passphrase,
							 key_len))
			return cmd_fail(0, "Invalid hex key\n");
		ret = luks_unlock(dev_desc->bdev, &info, key_buf, key_len,
				  true, master_key, &key_size);
	} else {
		/* Unlock with passphrase */
		ret = luks_unlock(dev_desc->bdev, &info, (const u8 *)passphrase,
				  strlen(passphrase), false, master_key,
				  &key_size);
	}
	if (ret)
		return cmd_fail(ret, UNLOCK_ERR, ret);

	/* Create blkmap device with label based on source device */
	snprintf(label, sizeof(label), "luks-%s-%s", iface, devpart);

	/* Create and map the blkmap device */
	ret = luks_create_blkmap(dev_desc->bdev, &info, master_key, key_size,
				 label, &blkmap_dev);
	if (ret) {
		ret = cmd_fail(ret, BLKMAP_ERR, ret);
		goto cleanup;
	}

	printf("Unlocked LUKS partition as blkmap device '%s'\n", label);

	ret = CMD_RET_SUCCESS;

cleanup:
	/* Wipe master key from stack */
	memset(master_key, '\0', sizeof(master_key));

	return ret;
}

static char luks_help_text[] =
	"detect <interface> <dev[:part]> - detect if partition is LUKS encrypted\n"
	"luks info <interface> <dev[:part]> - show LUKS header information\n"
	"luks unlock [-t | -p] <interface> <dev[:part]> <passphrase> - unlock LUKS partition\n"
	"  -t: Use TKey hardware security token with passphrase as USS\n"
	"  -p: Treat passphrase as hex-encoded pre-derived master key (skip KDF)\n";

U_BOOT_CMD_WITH_SUBCMDS(luks, "LUKS (Linux Unified Key Setup) operations",
			luks_help_text,
	U_BOOT_CMD_MKENT_NOOPTS(detect, 3, 1, do_luks_detect, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(info, 3, 1, do_luks_info, "", ""),
	U_BOOT_CMD_MKENT_GETOPT(unlock, 5, 1, do_luks_unlock, "", ""));
