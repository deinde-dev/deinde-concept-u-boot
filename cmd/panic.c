// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2020 Theobroma Systems Design und Consulting GmbH
 */

#include <vsprintf.h>
#include <command.h>
#include <getopt.h>

static int do_panic(struct getopt_state *gs)
{
	char *text = (gs->index < gs->argc) ? gs->argv[gs->index] : "";

	panic_str(text);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD_NOOPTS(
	panic,	2,	1,	do_panic,
	"Panic with optional message",
	"[message]"
);
