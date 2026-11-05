// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the fstypes command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

/* enough for the heading and every filesystem name U-Boot has */
#define MAX_LINE	200

/* Test that fstypes names the filesystems which are built in */
static int cmd_test_fstypes_base(struct unit_test_state *uts)
{
	char line[MAX_LINE];

	ut_assertok(run_command("fstypes", 0));
	ut_assert(console_record_readline(line, sizeof(line)) > 0);
	ut_assert_console_end();

	/*
	 * Which names appear depends on the drivers built in, so check the
	 * heading and then look for the ones this board must have
	 */
	ut_asserteq_strn("Supported filesystems: ", line);
	if (IS_ENABLED(CONFIG_FS_FAT))
		ut_assertnonnull(strstr(line, "fat"));
	if (IS_ENABLED(CONFIG_SANDBOX))
		ut_assertnonnull(strstr(line, "sandbox"));

	return 0;
}
CMD_TEST(cmd_test_fstypes_base, UTF_CONSOLE);

/* Test that fstypes refuses an argument, since it takes none */
static int cmd_test_fstypes_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("fstypes extra", 0));
	ut_assert_nextline("fstypes - List supported filesystem types");
	ut_assert_nextline("%s", "");
	ut_assert_nextline("Usage:");
	ut_assert_nextline("fstypes ");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_fstypes_usage, UTF_CONSOLE);
