// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018, Google Inc.
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <dm.h>
#include <getopt.h>
#include <spl.h>
#include <asm/cpu.h>
#include <asm/global_data.h>
#include <asm/state.h>
#include <dm/device-internal.h>
#include <dm/lists.h>

DECLARE_GLOBAL_DATA_PTR;

static int do_sb_handoff(struct getopt_state *gs)
{
#if IS_ENABLED(CONFIG_HANDOFF)
	if (gd->spl_handoff)
		printf("SPL handoff magic %lx\n", gd->spl_handoff->arch.magic);
	else
		printf("SPL handoff info not received\n");

	return 0;
#else
	cmd_err("Command not supported\n");

	return CMD_RET_USAGE;
#endif
}

static int do_sb_map(struct getopt_state *gs)
{
	sandbox_map_list();

	return 0;
}

static int do_sb_state(struct getopt_state *gs)
{
	struct sandbox_state *state;

	state = state_get_current();
	state_show(state);

	return 0;
}

static int do_sb_devon(struct getopt_state *gs)
{
	char *const *argv = gs->argv;
	int argc = gs->argc;
	struct udevice *dev;
	ofnode root, node;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	/* Find the specified device tree node */
	root = oftree_root(oftree_default());
	node = ofnode_find_subnode(root, argv[1]);
	if (!ofnode_valid(node))
		return cmd_fail(0, "Device tree node '%s' not found\n",
				argv[1]);

	/* Check if device is already bound */
	ret = device_find_global_by_ofnode(node, &dev);
	if (!ret)
		return cmd_fail(0, "Device '%s' is already enabled\n", argv[1]);

	/* Bind the device from device tree */
	ret = lists_bind_fdt(gd->dm_root, node, &dev, NULL, false);
	if (ret)
		return cmd_fail(ret, "Failed to bind device '%s' (err %dE)\n",
				argv[1], ret);

	/* Probe the device */
	ret = device_probe(dev);
	if (ret)
		return cmd_fail(ret, "Failed to probe device '%s' (err %dE)\n",
				argv[1], ret);

	printf("Device '%s' enabled\n", dev->name);

	return CMD_RET_SUCCESS;
}

static int do_sb_grid(struct getopt_state *gs)
{
	struct sandbox_state *state = state_get_current();
	char *const *argv = gs->argv;
	int argc = gs->argc;

	if (argc < 2)
		return CMD_RET_USAGE;

	state->show_grid = hextoul(argv[1], NULL);
	if (argc >= 3)
		state->grid_size = hextoul(argv[2], NULL);

	return 0;
}

static int do_sb_devoff(struct getopt_state *gs)
{
	char *const *argv = gs->argv;
	int argc = gs->argc;
	struct udevice *dev;
	ofnode root, node;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	/* Find the specified device tree node */
	root = oftree_root(oftree_default());
	node = ofnode_find_subnode(root, argv[1]);
	if (!ofnode_valid(node))
		return cmd_fail(0, "Device tree node '%s' not found\n",
				argv[1]);

	/* Find the device bound to this node */
	ret = device_find_global_by_ofnode(node, &dev);
	if (ret)
		return cmd_fail(ret,
				"Device '%s' not found or not bound (err %dE)\n",
				argv[1], ret);

	/* Remove the device (deactivate it) */
	ret = device_remove(dev, DM_REMOVE_NORMAL);
	if (ret)
		return cmd_fail(ret, "Failed to remove device '%s' (err %dE)\n",
				argv[1], ret);

	/* Unbind the device */
	ret = device_unbind(dev);
	if (ret)
		return cmd_fail(ret, "Failed to unbind device '%s' (err %dE)\n",
				argv[1], ret);

	printf("Device '%s' disabled\n", argv[1]);

	return CMD_RET_SUCCESS;
}

U_BOOT_LONGHELP(sb,
	"devoff <node>  - Disable device from device tree node\n"
	"sb devon <node>   - Enable device from device tree node\n"
	"sb grid <0|1> [<size>] - Enable/disable grid overlay on video\n"
	"sb handoff        - Show handoff data received from SPL\n"
	"sb map            - Show mapped memory\n"
	"sb state          - Show sandbox state");

U_BOOT_CMD_WITH_SUBCMDS(sb, "Sandbox status commands", sb_help_text,
	U_BOOT_CMD_MKENT_NOOPTS(devoff, 2, 1, do_sb_devoff, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(devon, 2, 1, do_sb_devon, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(grid, 3, 1, do_sb_grid, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(handoff, 1, 1, do_sb_handoff, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(map, 1, 1, do_sb_map, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(state, 1, 1, do_sb_state, "", ""));
