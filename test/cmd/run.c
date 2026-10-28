// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the run command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <env.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Test 'run' with variables holding commands */
static int cmd_test_run_base(struct unit_test_state *uts)
{
	ut_assertok(env_set("ut_run_one", "echo one"));
	ut_assertok(env_set("ut_run_two", "echo two"));

	/* a variable holding a single command */
	ut_assertok(run_command("run ut_run_one", 0));
	ut_assert_nextline("one");
	ut_assert_console_end();

	/* each name is run in turn */
	ut_assertok(run_command("run ut_run_one ut_run_two", 0));
	ut_assert_nextline("one");
	ut_assert_nextline("two");
	ut_assert_console_end();

	/* a variable may hold several commands */
	ut_assertok(env_set("ut_run_both", "echo three; echo four"));
	ut_assertok(run_command("run ut_run_both", 0));
	ut_assert_nextline("three");
	ut_assert_nextline("four");
	ut_assert_console_end();

	/* and may run another variable */
	ut_assertok(env_set("ut_run_outer", "run ut_run_two"));
	ut_assertok(run_command("run ut_run_outer", 0));
	ut_assert_nextline("two");
	ut_assert_console_end();

	ut_assertok(env_set("ut_run_one", NULL));
	ut_assertok(env_set("ut_run_two", NULL));
	ut_assertok(env_set("ut_run_both", NULL));
	ut_assertok(env_set("ut_run_outer", NULL));

	return 0;
}
CMD_TEST(cmd_test_run_base, UTF_CONSOLE);

/* Test 'run' with a name which is not an environment variable */
static int cmd_test_run_missing(struct unit_test_state *uts)
{
	ut_assertok(env_set("ut_run_two", "echo two"));

	/* a name which is not defined is an error */
	ut_asserteq(1, run_command("run ut_run_absent", 0));
	ut_assert_nextline("## Error: \"ut_run_absent\" not defined");
	ut_assert_console_end();

	/* it stops the command, so the name after it is not run */
	ut_asserteq(1, run_command("run ut_run_absent ut_run_two", 0));
	ut_assert_nextline("## Error: \"ut_run_absent\" not defined");
	ut_assert_console_end();

	/* a failing script stops it too, and its return value comes back */
	ut_assertok(env_set("ut_run_bad", "run ut_run_absent"));
	ut_asserteq(1, run_command("run ut_run_bad ut_run_two", 0));
	ut_assert_nextline("## Error: \"ut_run_absent\" not defined");
	ut_assert_console_end();

	ut_assertok(env_set("ut_run_two", NULL));
	ut_assertok(env_set("ut_run_bad", NULL));

	return 0;
}
CMD_TEST(cmd_test_run_missing, UTF_CONSOLE);

/**
 * check_usage() - Check that the usage message is the only console output
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("run - run commands in an environment variable");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("run var [...]");
	ut_assert_nextline(
		"    - run the commands in the environment variable(s) 'var'");
	ut_assert_console_end();

	return 0;
}

/* Test 'run' with no variable to run */
static int cmd_test_run_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("run", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_run_usage, UTF_CONSOLE);
