// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the mmcinfo command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <test/cmd.h>
#include <test/ut.h>

/**
 * select_mmc0() - Make the first emulated MMC device the current one
 *
 * The command reports on the current device, which an earlier test may have
 * changed. Also, find_mmc_device() needs the device to be probed, which
 * 'mmc list' does, so neither 'mmc dev' nor mmcinfo can reach it on its own.
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int select_mmc0(struct unit_test_state *uts)
{
	ut_assertok(run_command("mmc list", 0));
	ut_assertok(run_command("mmc dev 0", 0));
	console_record_reset();

	return 0;
}

/**
 * check_usage() - Check the usage message the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("mmcinfo - display MMC info");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("mmcinfo - display info of the current MMC device");
	}
	ut_assert_console_end();

	return 0;
}

/* Test the report for the first emulated MMC device */
static int cmd_test_mmcinfo_base(struct unit_test_state *uts)
{
	ut_assertok(select_mmc0(uts));

	ut_assertok(run_command("mmcinfo", 0));
	ut_assert_nextline("Device: mmc0");
	ut_assert_nextline("Manufacturer ID: 0");
	ut_assert_nextline("OEM: 0");

	/*
	 * The emulated card has an all-zero CID, so the name is six NUL bytes
	 * and printf() stops at the first one, dropping the newline with it.
	 * The bus speed therefore shares the line
	 */
	ut_assert_nextline("Name: Bus Speed: 1000000");
	if (IS_ENABLED(CONFIG_MMC_VERBOSE))
		ut_assert_nextline("Mode: MMC legacy");
	ut_assert_nextline("Rd Block Len: 512");
	ut_assert_nextline("SD version 3.0");
	ut_assert_nextline("High Capacity: Yes");
	ut_assert_nextline("Capacity: 1 MiB");
	ut_assert_nextline("Bus Width: 1-bit");
	if (IS_ENABLED(CONFIG_MMC_WRITE))
		ut_assert_nextline("Erase Group Size: 512 Bytes");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mmcinfo_base, UTF_CONSOLE);

/* Test that 'mmc info' reports the same thing */
static int cmd_test_mmcinfo_subcmd(struct unit_test_state *uts)
{
	ut_assertok(select_mmc0(uts));

	ut_assertok(run_command("mmc info", 0));
	ut_assert_nextline("Device: mmc0");
	ut_assert_skip_to_line("Capacity: 1 MiB");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_mmcinfo_subcmd, UTF_CONSOLE);

/* Test that the command refuses an argument and an option */
static int cmd_test_mmcinfo_usage(struct unit_test_state *uts)
{
	ut_assertok(select_mmc0(uts));

	ut_asserteq(1, run_command("mmcinfo extra", 0));
	ut_assertok(check_usage(uts));

	/* the command has no options, so one is refused as well */
	ut_asserteq(1, run_command("mmcinfo -x", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_mmcinfo_usage, UTF_CONSOLE);
