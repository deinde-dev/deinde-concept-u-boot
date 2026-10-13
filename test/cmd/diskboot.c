// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the diskboot command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <errno.h>
#include <test/cmd.h>
#include <test/ut.h>

/* the load address the tests would read into */
#define DISKBOOT_ADDR	"1000"

/*
 * Sandbox has no IDE controller, so the paths these tests cover are the ones
 * before a drive is reached. They hold whatever the board has attached, since
 * no drive is ever named: the device specification is either missing or names
 * a drive number no controller can offer.
 *
 * The bootdevice environment variable is cleared rather than assumed to be
 * unset, since test.py keeps the environment from one run to the next.
 *
 * Both messages come from blk_get_device_part_str(), which is a stub returning
 * an error without printing anything where CONFIG_PARTITIONS is off, as it is
 * on the malta boards. The command can do nothing at all there, so the tests
 * which expect a message skip.
 */

/* Test the device specifications which name nothing to boot from */
static int cmd_test_diskboot_base(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_PARTITIONS))
		return -EAGAIN;

	ut_assertok(run_command("setenv bootdevice", 0));

	/* the default device comes from the environment, which is now unset */
	ut_asserteq(1, run_command("diskboot", 0));
	ut_assert_nextline("** No device specified **");
	ut_assert_console_end();

	/* an address on its own leaves the device just as unspecified */
	ut_asserteq(1, run_command("diskboot " DISKBOOT_ADDR, 0));
	ut_assert_nextline("** No device specified **");
	ut_assert_console_end();

	ut_asserteq(1, run_command("diskboot " DISKBOOT_ADDR " 99:1", 0));
	ut_assert_nextline("** Bad device specification ide 99 **");
	ut_assert_console_end();

	/* a '-' means the same as leaving the argument out */
	ut_asserteq(1, run_command("diskboot " DISKBOOT_ADDR " -", 0));
	ut_assert_nextline("** No device specified **");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_diskboot_base, UTF_CONSOLE);

/* Test that the device defaults to the bootdevice environment variable */
static int cmd_test_diskboot_bootdevice(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_PARTITIONS))
		return -EAGAIN;

	ut_assertok(run_command("setenv bootdevice 99:1", 0));

	ut_asserteq(1, run_command("diskboot " DISKBOOT_ADDR, 0));
	ut_assert_nextline("** Bad device specification ide 99 **");
	ut_assert_console_end();

	/* the command line wins where both are given */
	ut_asserteq(1, run_command("diskboot " DISKBOOT_ADDR " 98:1", 0));
	ut_assert_nextline("** Bad device specification ide 98 **");
	ut_assert_console_end();

	ut_assertok(run_command("setenv bootdevice", 0));

	return 0;
}
CMD_TEST(cmd_test_diskboot_bootdevice, UTF_CONSOLE);

/**
 * check_usage() - Check the usage message the command prints
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("diskboot - boot from IDE device");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("diskboot loadAddr dev:part");
	ut_assert_console_end();

	return 0;
}

/* Test the command lines which the command refuses */
static int cmd_test_diskboot_usage(struct unit_test_state *uts)
{
	/* the command takes two arguments at most */
	ut_asserteq(1, run_command("diskboot " DISKBOOT_ADDR " 0:1 2", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_diskboot_usage, UTF_CONSOLE);
