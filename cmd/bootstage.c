// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 */

#include <bootstage.h>
#include <command.h>
#include <env.h>
#include <getopt.h>
#include <mapmem.h>
#include <vsprintf.h>
#include <linux/string.h>

static int do_bootstage_report(struct getopt_state *gs)
{
	bootstage_report();

	return 0;
}

#if IS_ENABLED(CONFIG_BOOTSTAGE_STASH)
static int get_base_size(int argc, char *const argv[], ulong *basep,
			 ulong *sizep)
{
	char *endp;

	*basep = CONFIG_BOOTSTAGE_STASH_ADDR;
	*sizep = CONFIG_BOOTSTAGE_STASH_SIZE;
	if (argc < 2)
		return 0;
	*basep = hextoul(argv[1], &endp);
	if (*argv[1] == 0 || *endp != 0)
		return -1;
	if (argc == 2)
		return 0;
	*sizep = hextoul(argv[2], &endp);
	if (*argv[2] == 0 || *endp != 0)
		return -1;

	return 0;
}

static int do_bootstage_stash(struct getopt_state *gs)
{
	int argc = gs->argc;
	char *const *argv = gs->argv;
	ulong base, size;
	void *ptr;
	int ret;

	if (get_base_size(argc, argv, &base, &size))
		return CMD_RET_USAGE;
	if (base == -1UL)
		return cmd_fail(0, "No bootstage stash area defined\n");

	ptr = map_sysmem(base, size);
	if (0 == strcmp(argv[0], "stash"))
		ret = bootstage_stash(ptr, size);
	else
		ret = bootstage_unstash(ptr, size);
	unmap_sysmem(ptr);
	if (ret)
		return 1;

	return 0;
}
#endif

static int __maybe_unused do_bootstage_save(struct getopt_state *gs)
{
	env_set_hex("bootstage_count", bootstage_get_rec_count());

	return 0;
}

static int __maybe_unused do_bootstage_restore(struct getopt_state *gs)
{
	ulong count = env_get_hex("bootstage_count", 0);

	if (count)
		bootstage_set_rec_count(count);

	return 0;
}

static struct cmd_tbl cmd_bootstage_sub[] = {
	U_BOOT_CMD_MKENT_NOOPTS(report, 2, 1, do_bootstage_report, "", ""),
#if IS_ENABLED(CONFIG_BOOTSTAGE_SAVE)
	U_BOOT_CMD_MKENT_NOOPTS(save, 1, 0, do_bootstage_save, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(restore, 1, 0, do_bootstage_restore, "", ""),
#endif
#if IS_ENABLED(CONFIG_BOOTSTAGE_STASH)
	U_BOOT_CMD_MKENT_NOOPTS(stash, 4, 0, do_bootstage_stash, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(unstash, 4, 0, do_bootstage_stash, "", ""),
#endif
};

/*
 * Process a bootstage sub-command
 */
static int do_bootstage(struct getopt_state *gs)
{
	return cmd_dispatch_subcmd(gs, cmd_bootstage_sub,
				   ARRAY_SIZE(cmd_bootstage_sub));
}

U_BOOT_CMD_NOOPTS(
	bootstage, 4, 1, do_bootstage,
	"Boot stage command",
	" - check boot progress and timing\n"
	"report                      - Print a report\n"
#if IS_ENABLED(CONFIG_BOOTSTAGE_SAVE)
	"save                        - Save record count to $bootstage_count\n"
	"restore                     - Restore record count from $bootstage_count\n"
#endif
#if IS_ENABLED(CONFIG_BOOTSTAGE_STASH)
	"stash [<start> [<size>]]    - Stash data into memory\n"
	"unstash [<start> [<size>]]  - Unstash data from memory\n"
#endif
);
