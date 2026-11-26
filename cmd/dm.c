// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2013 Google, Inc
 *
 * (C) Copyright 2012
 * Marek Vasut <marex@denx.de>
 */

#include <command.h>
#include <getopt.h>
#include <dm/root.h>
#include <dm/util.h>

static int do_dm_dump_driver_compat(struct getopt_state *gs)
{
	dm_dump_driver_compat();

	return 0;
}

static int do_dm_dump_devres(struct getopt_state *gs)
{
	dm_dump_devres();

	return 0;
}

static int do_dm_dump_drivers(struct getopt_state *gs)
{
	dm_dump_drivers();

	return 0;
}

#if IS_ENABLED(CONFIG_DM_STATS)
static int do_dm_dump_mem(struct getopt_state *gs)
{
	struct dm_stats mem;

	dm_get_mem(&mem);
	dm_dump_mem(&mem);

	return 0;
}
#endif /* DM_STATS */

static int do_dm_dump_static_driver_info(struct getopt_state *gs)
{
	dm_dump_static_driver_info();

	return 0;
}

static int do_dm_dump_tree(struct getopt_state *gs)
{
	bool extended = false, sort = false;
	char *device = NULL;
	int opt;

	while ((opt = getopt(gs, "+es")) > 0) {
		switch (opt) {
		case 'e':
			extended = true;
			break;
		case 's':
			sort = true;
			break;
		default:
			return CMD_RET_USAGE;
		}
	}

	/* a second name would otherwise be ignored */
	if (IS_ENABLED(CONFIG_CMD_ARG_CHECK) && gs->argc - gs->index > 1)
		return CMD_RET_USAGE;

	if (gs->index < gs->argc)
		device = gs->argv[gs->index];

	dm_dump_tree(device, extended, sort);

	return 0;
}

static int do_dm_dump_uclass(struct getopt_state *gs)
{
	bool extended = false;
	char *uclass = NULL;
	int opt;

	while ((opt = getopt(gs, "+e")) > 0) {
		switch (opt) {
		case 'e':
			extended = true;
			break;
		default:
			return CMD_RET_USAGE;
		}
	}

	/* a second name would otherwise be ignored */
	if (IS_ENABLED(CONFIG_CMD_ARG_CHECK) && gs->argc - gs->index > 1)
		return CMD_RET_USAGE;

	if (gs->index < gs->argc)
		uclass = gs->argv[gs->index];

	dm_dump_uclass(uclass, extended);

	return 0;
}

#if IS_ENABLED(CONFIG_DM_STATS)
#define DM_MEM_HELP	"dm mem           Provide a summary of memory usage\n"
#define DM_MEM		U_BOOT_CMD_MKENT_NOOPTS(mem, 1, 1, do_dm_dump_mem, \
						"", ""),
#else
#define DM_MEM_HELP
#define DM_MEM
#endif

U_BOOT_LONGHELP(dm,
	"compat        Dump list of drivers with compatibility strings\n"
	"dm devres        Dump list of device resources for each device\n"
	"dm drivers       Dump list of drivers with uclass and instances\n"
	DM_MEM_HELP
	"dm static        Dump list of drivers with static platform data\n"
	"dm tree [-s][-e][name]   Dump tree of driver model devices (-s=sort)\n"
	"dm uclass [-e][name]     Dump list of instances for each uclass");

U_BOOT_CMD_WITH_SUBCMDS(dm, "Driver model low level access", dm_help_text,
	U_BOOT_CMD_MKENT_NOOPTS(compat, 1, 1, do_dm_dump_driver_compat,
				"", ""),
	U_BOOT_CMD_MKENT_NOOPTS(devres, 1, 1, do_dm_dump_devres, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(drivers, 1, 1, do_dm_dump_drivers, "", ""),
	DM_MEM
	U_BOOT_CMD_MKENT_NOOPTS(static, 1, 1, do_dm_dump_static_driver_info,
				"", ""),
	U_BOOT_CMD_MKENT_GETOPT(tree, 4, 1, do_dm_dump_tree, "", ""),
	U_BOOT_CMD_MKENT_GETOPT(uclass, 3, 1, do_dm_dump_uclass, "", ""));
