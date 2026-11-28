// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the extension command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <env.h>
#include <mapmem.h>
#include <asm/global_data.h>
#include <linux/libfdt.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/* Check the listing of the two extension boards sandbox provides */
static int check_list(struct unit_test_state *uts)
{
	int i;

	for (i = 0; i < 2; i++) {
		ut_assert_nextline("Extension %d: extension board %d", i, i);
		ut_assert_nextline("\tManufacturer: \t\tsandbox");
		ut_assert_nextline("\tVersion: \t\t1.1");
		ut_assert_nextline("\tDevicetree overlay: \toverlay%d.dtbo", i);
		ut_assert_nextline(
			"\tOther information: \tFictional extension board");
	}
	ut_assert_console_end();

	return 0;
}

/* Check the usage message, which every bad argument shows */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("extension - Extension board management sub system");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline(
		"extension scan - scan plugged extension(s) board(s)");
	ut_assert_nextline(
		"extension list - lists available extension(s) board(s)");
	ut_assert_nextlinen("extension apply <extension number|all> - applies");
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}

/* Test scanning for extension boards and listing what is found */
static int cmd_test_extension_base(struct unit_test_state *uts)
{
	/* nothing is known about the extensions until the scan runs */
	ut_assertok(run_command("extension list", 0));
	ut_assert_nextline(
		"No extension registered - Please run \"extension scan\"");
	ut_assert_console_end();

	ut_assertok(run_command("extension scan", 0));
	ut_assert_nextline("Found 2 extension board(s).");
	ut_assert_console_end();

	ut_assertok(run_command("extension list", 0));
	ut_assertok(check_list(uts));

	/* a second scan starts the list again, rather than adding to it */
	ut_assertok(run_command("extension scan", 0));
	ut_assert_nextline("Found 2 extension board(s).");
	ut_assert_console_end();

	ut_assertok(run_command("extension list", 0));
	ut_assertok(check_list(uts));

	return 0;
}
CMD_TEST(cmd_test_extension_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA);

/* Test the usage message */
static int cmd_test_extension_usage(struct unit_test_state *uts)
{
	/* the command needs a sub-command */
	ut_asserteq(1, run_command("extension", 0));
	ut_assertok(check_usage(uts));

	/* an unknown sub-command shows the usage message too */
	ut_asserteq(1, run_command("extension fish", 0));
	ut_assertok(check_usage(uts));

	/* 'apply' has to be told what to apply */
	ut_asserteq(1, run_command("extension apply", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_extension_usage, UTF_CONSOLE);

/* Test that a sub-command refuses an argument it has no use for */
static int cmd_test_extension_strict(struct unit_test_state *uts)
{
	/* 'scan' takes no arguments */
	ut_asserteq(1, run_command("extension scan extra", 0));
	ut_assertok(check_usage(uts));

	/* nor any options, which are refused only with CMD_ARG_CHECK */
	if (IS_ENABLED(CONFIG_CMD_ARG_CHECK)) {
		ut_asserteq(1, run_command("extension scan -x", 0));
		ut_assertok(check_usage(uts));
	}

	/* 'apply' takes only the extension to apply */
	ut_asserteq(1, run_command("extension apply 0 1", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_extension_strict, UTF_CONSOLE);

/* Test that a repeat only runs a sub-command which allows one */
static int cmd_test_extension_repeat(struct unit_test_state *uts)
{
	char *const scan_args[] = { "extension", "scan", NULL };
	char *const list_args[] = { "extension", "list", NULL };
	int repeatable;

	/* pressing Enter after a scan scans again */
	repeatable = -1;
	ut_assertok(cmd_process(0, 2, scan_args, &repeatable, NULL));
	ut_assert_nextline("Found 2 extension board(s).");
	ut_asserteq(1, repeatable);
	ut_assert_console_end();

	/* the listing may not be repeated, and the dispatcher says so */
	repeatable = -1;
	ut_assertok(cmd_process(0, 2, list_args, &repeatable, NULL));
	ut_asserteq(0, repeatable);
	console_record_reset();

	/* so a repeat of it does nothing at all */
	ut_assertok(cmd_process(CMD_FLAG_REPEAT, 2, list_args, &repeatable,
				NULL));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_extension_repeat, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA);

/* Test what 'extension apply' reports when it cannot do its job */
static int cmd_test_extension_apply_err(struct unit_test_state *uts)
{
	struct fdt_header *old_fdt = working_fdt;
	char saved_cmd[256], saved_addr[32];
	const char *orig;

	/*
	 * Both variables may be left set by an earlier test, since sandbox
	 * keeps its environment in a file between runs
	 */
	orig = env_get("extension_overlay_cmd");
	*saved_cmd = '\0';
	if (orig)
		strlcpy(saved_cmd, orig, sizeof(saved_cmd));
	orig = env_get("extension_overlay_addr");
	*saved_addr = '\0';
	if (orig)
		strlcpy(saved_addr, orig, sizeof(saved_addr));
	ut_assertok(env_set("extension_overlay_cmd", NULL));
	ut_assertok(env_set("extension_overlay_addr", NULL));

	ut_assertok(run_command("extension scan", 0));
	ut_assert_nextline("Found 2 extension board(s).");
	ut_assert_console_end();

	/* the overlay has nowhere to go until 'fdt addr' has run */
	working_fdt = NULL;
	ut_asserteq(1, run_command("extension apply 0", 0));
	ut_assert_nextline("No FDT memory address configured. Please configure");
	ut_assert_nextline(
		"the FDT address via \"fdt addr <address>\" command.");
	ut_assert_console_end();

	/* point at the control devicetree, which these paths never touch */
	ut_assertok(run_commandf("fdt addr %lx",
				 (ulong)map_to_sysmem(gd->fdt_blob)));
	console_record_reset();

	/* sandbox has two extensions, numbered 0 and 1 */
	ut_asserteq(1, run_command("extension apply 2", 0));
	ut_assert_nextline("Wrong extension number");
	ut_assert_console_end();

	/* the command which loads an overlay comes from the environment */
	ut_asserteq(1, run_command("extension apply 0", 0));
	ut_assert_nextline("Environment extension_overlay_cmd is missing");
	ut_assert_console_end();

	/* the address to load it to comes from there as well */
	ut_assertok(env_set("extension_overlay_cmd", "echo never-run"));
	ut_asserteq(1, run_command("extension apply 0", 0));
	ut_assert_nextline("Environment extension_overlay_addr is missing");
	ut_assert_console_end();

	working_fdt = old_fdt;
	ut_assertok(env_set("extension_overlay_cmd",
			    *saved_cmd ? saved_cmd : NULL));
	ut_assertok(env_set("extension_overlay_addr",
			    *saved_addr ? saved_addr : NULL));

	return 0;
}
CMD_TEST(cmd_test_extension_apply_err,
	 UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA);
