// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the lsblk command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <test/cmd.h>
#include <test/ut.h>

/* the usage line, too long to write out where it is used */
#define HELP_LINE \
	"lsblk - display list of block device drivers and attached block devices"

/**
 * check_usage() - Check the usage message the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("lsblk - list block drivers and devices");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline(HELP_LINE);
	}
	ut_assert_console_end();

	return 0;
}

/* Test that the listing has a heading and a line for each block driver */
static int cmd_test_lsblk_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("lsblk", 0));
	ut_assert_nextline("Block Driver          Devices");
	ut_assert_nextline("-----------------------------");

	/*
	 * Drivers come out in linker-list order, which is by name, so the mmc
	 * driver with its three emulated devices comes before the host one,
	 * which has nothing bound until 'host bind' runs
	 */
	ut_assert_skip_to_line("mmc_blk             : mmc 2, mmc 1, mmc 0");
	ut_assert_skip_to_line("sandbox_host_blk    : <none>");

	/* the drivers after that depend on the board, so stop checking here */
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_lsblk_base, UTF_CONSOLE);

/* Test that the command refuses an argument and an option */
static int cmd_test_lsblk_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("lsblk extra", 0));
	ut_assertok(check_usage(uts));

	/* the command has no options, so one is refused as well */
	ut_asserteq(1, run_command("lsblk -x", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_lsblk_usage, UTF_CONSOLE);
