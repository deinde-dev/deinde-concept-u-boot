// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

/*
 * bootd - boot default, i.e. run the command in the "bootcmd" environment
 * variable
 */
#include <command.h>
#include <env.h>
#include <getopt.h>

static int do_bootd(struct getopt_state *gs)
{
	if (gs->cmd_flag & CMD_FLAG_BOOTD) {
		puts("'bootd' recursion detected\n");
		return CMD_RET_FAILURE;
	}

	return run_command(env_get("bootcmd"), gs->cmd_flag | CMD_FLAG_BOOTD);
}

U_BOOT_CMD_NOOPTS(
	boot,	1,	1,	do_bootd,
	"boot default, i.e., run 'bootcmd'",
	""
);

/* keep old command name "bootd" for backward compatibility */
U_BOOT_CMD_NOOPTS(
	bootd, 1,	1,	do_bootd,
	"boot default, i.e., run 'bootcmd'",
	""
);
