// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Canonical Ltd
 *
 * Command for communicating with Tillitis TKey to create wrapping keys
 * from user-provided passwords.
 */

#include <command.h>
#include <console.h>
#include <dm.h>
#include <getopt.h>
#include <hexdump.h>
#include <malloc.h>
#include <time.h>
#include <tkey.h>
#include <asm/unaligned.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/errno.h>

/* Static device pointer set by tkey connect command */
static struct udevice *tkey_dev;

/* What 'tkey connect' reports for a name which no device has */
#define ERR_NO_SUCH_DEV	"Failed to find TKey device '%s' (err %dE)\n"

struct udevice *tkey_get_device(void)
{
	struct udevice *dev;
	int ret;

	/* If a device was set by tkey connect, return it */
	if (tkey_dev)
		return tkey_dev;

	ret = uclass_first_device_err(UCLASS_TKEY, &dev);
	if (ret) {
		cmd_err("No device found (err %dE)\n", ret);
		return NULL;
	}

	return dev;
}

static void print_hex(const char *label, const u8 *data, size_t len)
{
	size_t i;

	printf("%s: ", label);
	for (i = 0; i < len; i++)
		printf("%02x", data[i]);
	printf("\n");
}

static int do_tkey_connect(struct getopt_state *gs)
{
	struct udevice *dev;
	int ret;

	/* Check if device name is provided as optional first argument */
	if (gs->argc > 1) {
		const char *dev_name = gs->argv[1];

		ret = uclass_get_device_by_name(UCLASS_TKEY, dev_name, &dev);
		if (ret)
			return cmd_fail(ret, ERR_NO_SUCH_DEV, dev_name, ret);
	} else {
		ret = uclass_first_device_err(UCLASS_TKEY, &dev);
		if (ret)
			return cmd_fail(ret, "No device found (err %dE)\n",
					ret);
	}

	/* Set the static device pointer for subsequent commands */
	tkey_dev = dev;

	printf("Connected to TKey device\n");

	return CMD_RET_SUCCESS;
}

static int do_tkey_info(struct getopt_state *gs)
{
	char name0[TKEY_NAME_SIZE], name1[TKEY_NAME_SIZE];
	u8 udi[TKEY_UDI_SIZE];
	struct udevice *dev;
	u32 version;
	int ret;

	dev = tkey_get_device();
	if (!dev)
		return CMD_RET_FAILURE;

	ret = tkey_get_name_version(dev, name0, name1, &version);
	if (ret)
		return cmd_fail(ret, "Failed to get device info (err %dE)\n",
				ret);

	printf("Name0: %.4s Name1: %.4s Version: %u\n", name0, name1, version);

	ret = tkey_get_udi(dev, udi);
	if (ret) {
		if (ret == -ENOTSUPP)
			return cmd_fail(ret,
					"UDI not available - replug device\n");

		return cmd_fail(ret, "Failed to get UDI (err %dE)\n", ret);
	}
	print_hex("UDI", udi, TKEY_UDI_SIZE);

	return CMD_RET_SUCCESS;
}

static int do_tkey_wrapkey(struct getopt_state *gs)
{
	u8 wrapping_key[TKEY_WRAPPING_KEY_SIZE];
	const char *password;
	struct udevice *dev;
	int ret;

	if (gs->argc != 2)
		return CMD_RET_USAGE;

	dev = tkey_get_device();
	if (!dev)
		return CMD_RET_FAILURE;

	password = gs->argv[1];

	ret = tkey_derive_wrapping_key(dev, password, wrapping_key);
	if (ret) {
		if (ret == -ENOTSUPP)
			return cmd_fail(ret,
					"UDI not available - replug device\n");

		return cmd_fail(ret, "Cannot derive wrapping key (err %dE)\n",
				ret);
	}

	print_hex("Wrapping Key", wrapping_key, TKEY_WRAPPING_KEY_SIZE);

	return CMD_RET_SUCCESS;
}

static int do_tkey_fwmode(struct getopt_state *gs)
{
	struct udevice *dev;
	int ret;

	dev = tkey_get_device();
	if (!dev)
		return CMD_RET_FAILURE;

	ret = tkey_in_app_mode(dev);
	if (ret < 0)
		return cmd_fail(ret, "Failed to check device mode (err %dE)\n",
				ret);

	if (!ret)
		printf("firmware mode\n");
	else
		printf("app mode\n");

	return CMD_RET_SUCCESS;
}

static int do_tkey_signer(struct getopt_state *gs)
{
	printf("signer binary: %ld bytes at %p-%p\n", (long)TKEY_SIGNER_SIZE,
	       __signer_1_0_0_begin, __signer_1_0_0_end);

	return CMD_RET_SUCCESS;
}

static int do_tkey_getkey(struct getopt_state *gs)
{
	const char *hash = NULL;
	u8 expect[TKEY_HASH_SIZE];
	u8 disk_key[TKEY_DISK_KEY_SIZE];
	u8 key_hash[TKEY_HASH_SIZE];
	u8 pubkey[TKEY_PUBKEY_SIZE];
	char *const *argv = gs->argv;
	int argc = gs->argc;
	bool verify = false;
	struct udevice *dev;
	const char *uss;
	u32 uss_len;
	int ret;

	if (argc != 2 && argc != 3)
		return CMD_RET_USAGE;

	dev = tkey_get_device();
	if (!dev)
		return CMD_RET_FAILURE;

	uss = argv[1];
	uss_len = strlen(uss);
	if (uss_len > TKEY_USS_MAX_SIZE)
		return cmd_fail(0, "USS too long (max %d bytes, got %u)\n",
				TKEY_USS_MAX_SIZE, uss_len);

	/* Check if verification hash is provided */
	if (argc == 3) {
		int i;

		hash = argv[2];
		verify = true;

		/* Convert hex string to bytes */
		if (strlen(hash) != TKEY_HASH_SIZE * 2) {
			cmd_err("Verification hash must be %d hex chars\n",
				TKEY_HASH_SIZE * 2);
			return CMD_RET_USAGE;
		}

		for (i = 0; i < TKEY_HASH_SIZE; i++)
			expect[i] = (hex_to_bin(hash[i * 2]) << 4) |
				     hex_to_bin(hash[i * 2 + 1]);
	}

	/* Derive disk key using uclass function */
	ret = tkey_derive_disk_key(dev, (const u8 *)__signer_1_0_0_begin,
				   TKEY_SIGNER_SIZE, (const u8 *)uss,
				   uss_len, disk_key, pubkey, key_hash);
	if (ret)
		return cmd_fail(ret, "Failed to derive disk key (err %dE)\n",
				ret);

	/* Display results */
	print_hex("Public Key", pubkey, TKEY_PUBKEY_SIZE);
	print_hex("Disk Key", disk_key, TKEY_DISK_KEY_SIZE);

	/* Verify or display verification hash */
	if (verify) {
		/* Verify USS by comparing hashes */
		if (memcmp(key_hash, expect, TKEY_HASH_SIZE) == 0) {
			printf("\npassword correct\n");
		} else {
			printf("\nwrong password\n");
			print_hex("Expected", expect, TKEY_HASH_SIZE);
			print_hex("Got", key_hash, TKEY_HASH_SIZE);
			return CMD_RET_FAILURE;
		}
	} else {
		print_hex("Verification Hash", key_hash, TKEY_HASH_SIZE);
		/* to verify USS later: tkey getkey <uss> <verification-hash> */
	}

	return CMD_RET_SUCCESS;
}

static int do_tkey_loadapp(struct getopt_state *gs)
{
	struct udevice *dev;
	const char *uss = NULL;
	u32 ulen = 0;
	int ret;

	dev = tkey_get_device();
	if (!dev)
		return CMD_RET_FAILURE;

	/* Optional USS parameter */
	if (gs->argc == 2) {
		uss = gs->argv[1];
		ulen = strlen(uss);
		if (ulen > TKEY_USS_MAX_SIZE)
			return cmd_fail(0,
					"USS too long (max %d bytes, got %u)\n",
					TKEY_USS_MAX_SIZE, ulen);
	}

	printf("Loading signer app (%ld bytes)%s...", (long)TKEY_SIGNER_SIZE,
	       uss ? " with USS" : "");
	ret = tkey_load_app_with_uss(dev, (const u8 *)__signer_1_0_0_begin,
				     TKEY_SIGNER_SIZE, (const u8 *)uss, ulen);
	if (ret) {
		if (ret == -ENOTSUPP)
			return cmd_fail(ret, "Invalid mode - replug device?\n");

		return cmd_fail(ret, "Failed to load app (err %dE)\n", ret);
	}
	printf("done\n");

	return CMD_RET_SUCCESS;
}

U_BOOT_LONGHELP(tkey,
	"connect [device-name] - Connect to TKey device\n"
	"    Optional device-name to connect to specific TKey device\n"
	"tkey fwmode     - Check if device is in firmware or app mode\n"
	"tkey getkey <uss> [verify-hash] - Get disk encryption key\n"
	"    Loads app with USS, derives key. Same USS always produces same key.\n"
	"    Optional verify-hash checks if USS is correct\n"
	"tkey info       - Show TKey device information\n"
	"tkey loadapp [uss] - Load embedded signer app to TKey\n"
	"    Firmware mode only. Optional USS for key derivation\n"
	"tkey signer     - Show embedded signer binary information\n"
	"tkey wrapkey <password> - Create wrapping key from password and UDI");

U_BOOT_CMD_WITH_SUBCMDS(tkey, "Tillitis TKey security token operations",
			tkey_help_text,
	U_BOOT_CMD_MKENT_NOOPTS(connect, 2, 1, do_tkey_connect, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(fwmode, 1, 1, do_tkey_fwmode, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(getkey, 3, 1, do_tkey_getkey, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(info, 1, 1, do_tkey_info, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(loadapp, 2, 1, do_tkey_loadapp, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(signer, 1, 1, do_tkey_signer, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(wrapkey, 2, 1, do_tkey_wrapkey, "", ""));
