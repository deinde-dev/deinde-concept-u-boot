// SPDX-License-Identifier: GPL-2.0+
/*
 * LUKS (Linux Unified Key Setup) command
 *
 * Copyright (C) 2025 Canonical Ltd
 */

#include <blk.h>
#include <command.h>
#include <dm.h>
#include <hexdump.h>
#include <luks.h>
#include <part.h>
#include <tkey.h>
#include <u-boot/sha256.h>

static int do_luks_detect(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct blk_desc *dev_desc;
	struct disk_partition info;
	int part, ret, version;

	if (argc != 3)
		return CMD_RET_USAGE;

	part = blk_get_device_part_str(argv[1], argv[2], &dev_desc, &info, 1);
	if (part < 0)
		return CMD_RET_FAILURE;

	ret = luks_detect(dev_desc->bdev, &info);
	if (ret < 0) {
		printf("Not a LUKS partition (error %dE)\n", ret);
		return CMD_RET_FAILURE;
	}
	version = luks_get_version(dev_desc->bdev, &info);
	printf("LUKS%d encrypted partition detected\n", version);

	return CMD_RET_SUCCESS;
}

static int do_luks_info(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	struct blk_desc *dev_desc;
	struct disk_partition info;
	int part, ret;

	if (argc != 3)
		return CMD_RET_USAGE;

	part = blk_get_device_part_str(argv[1], argv[2], &dev_desc, &info, 1);
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
		printf("Failed to find TKey device\n");
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
		printf("Failed to derive TKey disk key (err %dE)\n", ret);
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

static int do_luks_unlock(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct blk_desc *dev_desc;
	struct disk_partition info;
	struct udevice *blkmap_dev;
	const char *passphrase = NULL;
	bool use_tkey = false;
	bool pre_derived = false;
	int part, ret, version;
	u8 master_key[128];
	char label[64];
	u32 key_size;

	/* Check for flags */
	while (argc > 1 && argv[1][0] == '-') {
		if (!strcmp(argv[1], "-t")) {
			use_tkey = true;
		} else if (!strcmp(argv[1], "-p")) {
			pre_derived = true;
		} else {
			return CMD_RET_USAGE;
		}
		argc--;
		argv++;
	}
	if (argc != 4)
		return CMD_RET_USAGE;

	part = blk_get_device_part_str(argv[1], argv[2], &dev_desc, &info, 1);
	if (part < 0)
		return CMD_RET_FAILURE;

	passphrase = argv[3];

	log_debug("Partition start %llx blks %llx blksz%lx\n",
		  (unsigned long long)info.start, (unsigned long long)info.size,
		  (ulong)dev_desc->blksz);

	/* Verify it's a LUKS partition */
	version = luks_get_version(dev_desc->bdev, &info);
	if (version < 0) {
		printf("Not a LUKS partition\n");
		return CMD_RET_FAILURE;
	}

	printf("Unlocking LUKS%d partition...\n", version);

	if (use_tkey) {
		ret = unlock_with_tkey(dev_desc, &info, passphrase, master_key,
				       &key_size);
	} else if (pre_derived) {
		/* Pre-derived key: passphrase is hex-encoded master key */
		u8 key_buf[64];
		size_t key_len = strlen(passphrase) / 2;

		if (key_len > sizeof(key_buf) || hex2bin(key_buf, passphrase,
							 key_len)) {
			printf("Invalid hex key\n");
			return CMD_RET_FAILURE;
		}
		ret = luks_unlock(dev_desc->bdev, &info, key_buf, key_len,
				  true, master_key, &key_size);
	} else {
		/* Unlock with passphrase */
		ret = luks_unlock(dev_desc->bdev, &info, (const u8 *)passphrase,
				  strlen(passphrase), false, master_key,
				  &key_size);
	}
	if (ret) {
		printf("Failed to unlock LUKS partition (err %dE)\n", ret);
		return CMD_RET_FAILURE;
	}

	/* Create blkmap device with label based on source device */
	snprintf(label, sizeof(label), "luks-%s-%s", argv[1], argv[2]);

	/* Create and map the blkmap device */
	ret = luks_create_blkmap(dev_desc->bdev, &info, master_key, key_size,
				 label, &blkmap_dev);
	if (ret) {
		printf("Failed to create blkmap device (err %dE)\n", ret);
		ret = CMD_RET_FAILURE;
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
	"luks unlock [-t] [-p] <interface> <dev[:part]> <passphrase> - unlock LUKS partition\n"
	"  -t: Use TKey hardware security token with passphrase as USS\n"
	"  -p: Treat passphrase as hex-encoded pre-derived master key (skip KDF)\n";

U_BOOT_CMD_WITH_SUBCMDS(luks, "LUKS (Linux Unified Key Setup) operations",
			luks_help_text,
	U_BOOT_SUBCMD_MKENT(detect, 3, 1, do_luks_detect),
	U_BOOT_SUBCMD_MKENT(info, 3, 1, do_luks_info),
	U_BOOT_SUBCMD_MKENT(unlock, 5, 1, do_luks_unlock));
