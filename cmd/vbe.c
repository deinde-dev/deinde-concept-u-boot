// SPDX-License-Identifier: GPL-2.0+
/*
 * Verified Boot for Embedded (VBE) command
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <bloblist.h>
#include <bootmeth.h>
#include <bootstd.h>
#include <command.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <spl.h>
#include <vbe.h>

static int do_vbe_list(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	vbe_list();

	return 0;
}

static int do_vbe_select(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	struct bootstd_priv *std;
	struct udevice *dev;
	int ret;

	ret = bootstd_get_priv(&std);
	if (ret)
		return CMD_RET_FAILURE;
	if (argc < 2) {
		std->vbe_bootmeth = NULL;
		return 0;
	}
	if (vbe_find_by_any(argv[1], &dev))
		return CMD_RET_FAILURE;

	std->vbe_bootmeth = dev;

	return 0;
}

static int do_vbe_info(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	struct bootstd_priv *std;
	char buf[256];
	int ret, len;

	ret = bootstd_get_priv(&std);
	if (ret)
		return CMD_RET_FAILURE;
	if (!std->vbe_bootmeth) {
		printf("No VBE bootmeth selected\n");
		return CMD_RET_FAILURE;
	}
	ret = bootmeth_get_state_desc(std->vbe_bootmeth, buf, sizeof(buf));
	if (ret) {
		printf("Failed (err=%d)\n", ret);
		return CMD_RET_FAILURE;
	}
	len = strnlen(buf, sizeof(buf));
	if (len >= sizeof(buf)) {
		printf("Buffer overflow\n");
		return CMD_RET_FAILURE;
	}

	puts(buf);
	if (buf[len] != '\n')
		putc('\n');

	return 0;
}

static int do_vbe_state(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	struct vbe_handoff *handoff = NULL;
	int i;

	if (IS_ENABLED(CONFIG_BLOBLIST)) {
		handoff = bloblist_find(BLOBLISTT_VBE,
					sizeof(struct vbe_handoff));
	}
	if (!handoff) {
		printf("No VBE state\n");
		return CMD_RET_FAILURE;
	}

	printf("Phases:");
	for (i = PHASE_NONE; i < PHASE_COUNT; i++) {
		if (handoff->phases & (1 << i)) {
			int margin = handoff->reloc_margin[i];

			printf(" %s", xpl_name(i));
			if (margin)
				printf(" (margin %x) ", margin);
		}
	}
	if (!handoff->phases)
		printf(" (none)");
	printf("\n");

	return 0;
}

/**
 * get_abrec() - Get the VBE bootmeth for the A/B/recovery subcommands
 *
 * This is the selected VBE bootmeth, or the first A/B/recovery one
 *
 * @devp: Returns the probed bootmeth
 * Return: 0 if OK, -ENOENT if there is none, other -ve on error
 */
static int get_abrec(struct udevice **devp)
{
	struct bootstd_priv *std;
	struct udevice *dev;
	int ret;

	ret = bootstd_get_priv(&std);
	if (ret)
		return ret;
	dev = std->vbe_bootmeth;
	if (!dev) {
		for (vbe_find_first_device(&dev); dev;
		     vbe_find_next_device(&dev)) {
			if (dev->driver == DM_DRIVER_GET(vbe_abrec))
				break;
		}
	}
	if (!dev)
		return -ENOENT;
	ret = device_probe(dev);
	if (ret) {
		printf("Cannot probe '%s' (err=%d)\n", dev->name, ret);
		return ret;
	}
	*devp = dev;

	return 0;
}

static int do_vbe_recovery(struct cmd_tbl *cmdtp, int flag, int argc,
			   char *const argv[])
{
	struct udevice *dev;
	bool recovery;
	int ret;

	if (!IS_ENABLED(CONFIG_BOOTMETH_VBE_ABREC)) {
		printf("Recovery is not supported\n");
		return CMD_RET_FAILURE;
	}
	ret = get_abrec(&dev);
	if (ret == -ENOENT)
		printf("No VBE bootmeth has recovery firmware\n");
	if (ret)
		return CMD_RET_FAILURE;

	if (argc > 1) {
		if (!strcmp(argv[1], "on"))
			recovery = true;
		else if (!strcmp(argv[1], "off"))
			recovery = false;
		else
			return CMD_RET_USAGE;
		ret = vbe_abrec_set_recovery(dev, recovery);
	} else {
		ret = vbe_abrec_get_recovery(dev, &recovery);
	}
	if (ret == -ENOSYS) {
		printf("VBE bootmeth '%s' has no recovery firmware\n", dev->name);
		return CMD_RET_FAILURE;
	} else if (ret) {
		printf("Failed (err=%d)\n", ret);
		return CMD_RET_FAILURE;
	}
	printf("Recovery: %s\n", recovery ? "on" : "off");

	return 0;
}

static int do_vbe_try(struct cmd_tbl *cmdtp, int flag, int argc,
		      char *const argv[])
{
	struct udevice *dev;
	bool try;
	int ret, count;

	if (!IS_ENABLED(CONFIG_BOOTMETH_VBE_ABREC)) {
		printf("Trying B is not supported\n");
		return CMD_RET_FAILURE;
	}
	ret = get_abrec(&dev);
	if (ret == -ENOENT)
		printf("No VBE bootmeth has B firmware\n");
	if (ret)
		return CMD_RET_FAILURE;

	ret = 0;
	if (argc > 1) {
		if (!strcmp(argv[1], "on"))
			try = true;
		else if (!strcmp(argv[1], "off"))
			try = false;
		else
			return CMD_RET_USAGE;
		ret = vbe_abrec_set_try(dev, try);
	}
	if (!ret)
		ret = vbe_abrec_get_try(dev, &try, &count);
	if (ret == -ENOSYS) {
		printf("VBE bootmeth '%s' has no B firmware\n", dev->name);
		return CMD_RET_FAILURE;
	} else if (ret) {
		printf("Failed (err=%d)\n", ret);
		return CMD_RET_FAILURE;
	}
	if (try)
		printf("Try B: yes, %d tries left\n", count);
	else
		printf("Try B: no\n");

	return 0;
}

U_BOOT_LONGHELP(vbe,
	"list   - list VBE bootmeths\n"
	"vbe select - select a VBE bootmeth by sequence or name\n"
	"vbe info   - show information about a VBE bootmeth\n"
	"vbe state  - show VBE state\n"
	"vbe recovery [on|off] - show or change whether to boot recovery firmware\n"
	"vbe try [on|off] - show or change whether to try the B firmware");

U_BOOT_CMD_WITH_SUBCMDS(vbe, "Verified Boot for Embedded", vbe_help_text,
	U_BOOT_SUBCMD_MKENT(list, 1, 1, do_vbe_list),
	U_BOOT_SUBCMD_MKENT(select, 2, 1, do_vbe_select),
	U_BOOT_SUBCMD_MKENT(state, 2, 1, do_vbe_state),
	U_BOOT_SUBCMD_MKENT(info, 2, 1, do_vbe_info),
	U_BOOT_SUBCMD_MKENT(recovery, 2, 1, do_vbe_recovery),
	U_BOOT_SUBCMD_MKENT(try, 2, 1, do_vbe_try));
