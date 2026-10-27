// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the version command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <version.h>
#include <version_string.h>
#include <test/cmd.h>
#include <test/ut.h>

/**
 * check_usage() - Check the usage message the command shows
 *
 * The 'Usage:' part comes from cmd_usage(), which only shows it with
 * CONFIG_SYS_LONGHELP. The last line is the command name followed by its
 * (empty) help text, so it carries a trailing space
 *
 * @uts: Test state
 * Return: 0 if OK, -ve on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("version - print monitor, compiler and linker version");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextlinen("version");
	}
	ut_assert_console_end();

	return 0;
}

/* Test 'version' */
static int cmd_test_version_base(struct unit_test_state *uts)
{
	/* a coreboot board shows its own version after the linker's */
	if (IS_ENABLED(CONFIG_SYS_COREBOOT))
		return -EAGAIN;

	ut_assertok(run_command("version", 0));

	/*
	 * a build which sets BUILD_TAG appends ', Build: <tag>' to the
	 * version string, so check only that the line starts with it
	 */
	ut_assert_nextlinen("%s", version_string);
	ut_assert_nextline_empty();
#ifdef CC_VERSION_STRING
	ut_assert_nextline("%s", CC_VERSION_STRING);
#endif
#ifdef LD_VERSION_STRING
	ut_assert_nextline("%s", LD_VERSION_STRING);
#endif
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_version_base, UTF_CONSOLE);

/* Test 'version' with an argument, which it does not take */
static int cmd_test_version_arg(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("version extra", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_version_arg, UTF_CONSOLE);

/* Test 'version' with an option, which it does not have either */
static int cmd_test_version_opt(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("version -x", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_version_opt, UTF_CONSOLE);
