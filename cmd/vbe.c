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
#include <getopt.h>
#include <spl.h>
#include <vbe.h>

static int do_vbe_list(struct getopt_state *gs)
{
	vbe_list();

	return 0;
}

static int do_vbe_select(struct getopt_state *gs)
{
	struct bootstd_priv *std;
	struct udevice *dev;
	int ret;

	ret = bootstd_get_priv(&std);
	if (ret)
		return CMD_RET_FAILURE;
	if (gs->argc < 2) {
		std->vbe_bootmeth = NULL;
		return 0;
	}
	if (vbe_find_by_any(gs->argv[1], &dev))
		return CMD_RET_FAILURE;

	std->vbe_bootmeth = dev;

	return 0;
}

static int do_vbe_info(struct getopt_state *gs)
{
	struct bootstd_priv *std;
	char buf[256];
	int ret, len;

	/* a stray argument would otherwise be ignored */
	if (IS_ENABLED(CONFIG_CMD_ARG_CHECK) && gs->index != gs->argc)
		return CMD_RET_USAGE;

	ret = bootstd_get_priv(&std);
	if (ret)
		return CMD_RET_FAILURE;
	if (!std->vbe_bootmeth)
		return cmd_fail(0, "No VBE bootmeth selected\n");
	ret = bootmeth_get_state_desc(std->vbe_bootmeth, buf, sizeof(buf));
	if (ret)
		return cmd_fail(ret, "Failed (err=%d)\n", ret);
	len = strnlen(buf, sizeof(buf));
	if (len >= sizeof(buf))
		return cmd_fail(0, "Buffer overflow\n");

	puts(buf);
	if (buf[len] != '\n')
		putc('\n');

	return 0;
}

static int do_vbe_state(struct getopt_state *gs)
{
	struct vbe_handoff *handoff = NULL;
	int i;

	/* a stray argument would otherwise be ignored */
	if (IS_ENABLED(CONFIG_CMD_ARG_CHECK) && gs->index != gs->argc)
		return CMD_RET_USAGE;

	if (IS_ENABLED(CONFIG_BLOBLIST)) {
		handoff = bloblist_find(BLOBLISTT_VBE,
					sizeof(struct vbe_handoff));
	}
	if (!handoff)
		return cmd_fail(0, "No VBE state\n");

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
		cmd_err("Cannot probe '%s' (err=%d)\n", dev->name, ret);
		return ret;
	}
	*devp = dev;

	return 0;
}

static int do_vbe_recovery(struct getopt_state *gs)
{
	struct udevice *dev;
	bool recovery;
	int ret;

	if (!IS_ENABLED(CONFIG_BOOTMETH_VBE_ABREC))
		return cmd_fail(0, "Recovery is not supported\n");
	ret = get_abrec(&dev);
	if (ret == -ENOENT)
		return cmd_fail(ret, "No VBE bootmeth has recovery firmware\n");
	if (ret)
		return CMD_RET_FAILURE;

	if (gs->argc > 1) {
		if (!strcmp(gs->argv[1], "on"))
			recovery = true;
		else if (!strcmp(gs->argv[1], "off"))
			recovery = false;
		else
			return CMD_RET_USAGE;
		ret = vbe_abrec_set_recovery(dev, recovery);
	} else {
		ret = vbe_abrec_get_recovery(dev, &recovery);
	}
	if (ret == -ENOSYS)
		return cmd_fail(ret,
				"VBE bootmeth '%s' has no recovery firmware\n",
				dev->name);
	else if (ret)
		return cmd_fail(ret, "Failed (err=%d)\n", ret);
	printf("Recovery: %s\n", recovery ? "on" : "off");

	return 0;
}

static int do_vbe_try(struct getopt_state *gs)
{
	struct udevice *dev;
	bool try;
	int ret, count;

	if (!IS_ENABLED(CONFIG_BOOTMETH_VBE_ABREC))
		return cmd_fail(0, "Trying B is not supported\n");
	ret = get_abrec(&dev);
	if (ret == -ENOENT)
		return cmd_fail(ret, "No VBE bootmeth has B firmware\n");
	if (ret)
		return CMD_RET_FAILURE;

	ret = 0;
	if (gs->argc > 1) {
		if (!strcmp(gs->argv[1], "on"))
			try = true;
		else if (!strcmp(gs->argv[1], "off"))
			try = false;
		else
			return CMD_RET_USAGE;
		ret = vbe_abrec_set_try(dev, try);
	}
	if (!ret)
		ret = vbe_abrec_get_try(dev, &try, &count);
	if (ret == -ENOSYS)
		return cmd_fail(ret, "VBE bootmeth '%s' has no B firmware\n",
				dev->name);
	else if (ret)
		return cmd_fail(ret, "Failed (err=%d)\n", ret);
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
	U_BOOT_CMD_MKENT_NOOPTS(list, 1, 1, do_vbe_list, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(select, 2, 1, do_vbe_select, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(state, 2, 1, do_vbe_state, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(info, 2, 1, do_vbe_info, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(recovery, 2, 1, do_vbe_recovery, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(try, 2, 1, do_vbe_try, "", ""));
