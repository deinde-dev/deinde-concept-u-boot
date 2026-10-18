// SPDX-License-Identifier: GPL-2.0+
/*
 * Command-line access to events
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <event.h>
#include <getopt.h>

static int do_event_list(struct getopt_state *gs)
{
	event_show_spy_list();

	return 0;
}

U_BOOT_LONGHELP(event,
	"list - list event spies");

U_BOOT_CMD_WITH_SUBCMDS(event, "Events", event_help_text,
	U_BOOT_CMD_MKENT_NOOPTS(list, 1, 1, do_event_list, "", ""));
