// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the bootd command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <env.h>
#include <errno.h>
#include <malloc.h>
#include <test/cmd.h>
#include <test/ut.h>

/**
 * save_bootcmd() - Take a copy of 'bootcmd' so that a test can change it
 *
 * The board's own boot command must come back afterwards, since the tests
 * which follow, and the boot after them, rely on it.
 *
 * @uts: Test state
 * @savedp: Returns a copy of the value, or NULL if it is unset
 */
static int save_bootcmd(struct unit_test_state *uts, char **savedp)
{
	const char *val = env_get("bootcmd");

	*savedp = NULL;
	if (val) {
		*savedp = strdup(val);
		ut_assertnonnull(*savedp);
	}

	return 0;
}

/**
 * restore_bootcmd() - Put 'bootcmd' back as it was
 *
 * @uts: Test state
 * @saved: Value from save_bootcmd(), which this frees
 */
static int restore_bootcmd(struct unit_test_state *uts, char *saved)
{
	ut_assertok(env_set("bootcmd", saved));
	free(saved);

	return 0;
}

/* Test that the command runs what 'bootcmd' holds */
static int cmd_test_bootd_base(struct unit_test_state *uts)
{
	char *saved;

	/* the tests below boot an 'echo', so the command must be present */
	if (!IS_ENABLED(CONFIG_CMD_ECHO))
		return -EAGAIN;

	ut_assertok(save_bootcmd(uts, &saved));
	ut_assertok(env_set("bootcmd", "echo booting the default"));

	ut_assertok(run_command("bootd", 0));
	ut_assert_nextline("booting the default");
	ut_assert_console_end();

	/* 'boot' is the same command under the name it is normally used by */
	ut_assertok(run_command("boot", 0));
	ut_assert_nextline("booting the default");
	ut_assert_console_end();

	/* a list of commands runs in order */
	ut_assertok(env_set("bootcmd", "echo first; echo second"));
	ut_assertok(run_command("bootd", 0));
	ut_assert_nextline("first");
	ut_assert_nextline("second");
	ut_assert_console_end();

	ut_assertok(restore_bootcmd(uts, saved));

	return 0;
}
CMD_TEST(cmd_test_bootd_base, UTF_CONSOLE);

/* Test that the command fails when there is nothing to boot */
static int cmd_test_bootd_unset(struct unit_test_state *uts)
{
	char *saved;

	ut_assertok(save_bootcmd(uts, &saved));
	ut_assertok(env_set("bootcmd", NULL));

	/* without a 'bootcmd' there is nothing to run, which is a failure */
	ut_asserteq(1, run_command("bootd", 0));
	ut_assert_console_end();

	ut_asserteq(1, run_command("boot", 0));
	ut_assert_console_end();

	ut_assertok(restore_bootcmd(uts, saved));

	return 0;
}
CMD_TEST(cmd_test_bootd_unset, UTF_CONSOLE);

/**
 * check_usage() - Check the message a command line the command refuses gives
 *
 * @uts: Test state
 * @name: Name the command is reached by
 */
static int check_usage(struct unit_test_state *uts, const char *name)
{
	ut_assert_nextline("%s - boot default, i.e., run 'bootcmd'", name);
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("%s ", name);
	}
	ut_assert_console_end();

	return 0;
}

/* Test that the command takes neither an argument nor an option */
static int cmd_test_bootd_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("bootd extra", 0));
	ut_assertok(check_usage(uts, "bootd"));

	ut_asserteq(1, run_command("boot -x", 0));
	ut_assertok(check_usage(uts, "boot"));

	return 0;
}
CMD_TEST(cmd_test_bootd_usage, UTF_CONSOLE);
