// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the askenv command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <env.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Test 'askenv' with just a variable name */
static int cmd_test_askenv_base(struct unit_test_state *uts)
{
	console_in_puts("value one\n");
	ut_assertok(run_command("askenv t_ask1", 0));
	ut_assert_nextline("Please enter 't_ask1': value one");
	ut_assert_console_end();
	ut_asserteq_str("value one", env_get("t_ask1"));

	/* the same thing is available as a subcommand of 'env' */
	console_in_puts("value two\n");
	ut_assertok(run_command("env ask t_ask1", 0));
	ut_assert_nextline("Please enter 't_ask1': value two");
	ut_assert_console_end();
	ut_asserteq_str("value two", env_get("t_ask1"));

	return 0;
}
CMD_TEST(cmd_test_askenv_base, UTF_CONSOLE);

/* Test 'askenv' with a message in place of the default one */
static int cmd_test_askenv_message(struct unit_test_state *uts)
{
	/* the words of the message are joined with spaces and one is added */
	console_in_puts("fred\n");
	ut_assertok(run_command("askenv t_ask2 Who goes there?", 0));
	ut_assert_nextline("Who goes there? fred");
	ut_assert_console_end();
	ut_asserteq_str("fred", env_get("t_ask2"));

	return 0;
}
CMD_TEST(cmd_test_askenv_message, UTF_CONSOLE);

/* Test 'askenv' with a size, which truncates the value */
static int cmd_test_askenv_size(struct unit_test_state *uts)
{
	/* a decimal size as the last argument is not part of the message */
	console_in_puts("1234567890123\n");
	ut_assertok(run_command("askenv t_ask3 Type a number: 10", 0));
	ut_assert_nextline("Type a number: 1234567890123");
	ut_assert_console_end();
	ut_asserteq_str("1234567890", env_get("t_ask3"));

	/* with no message the default one still appears */
	console_in_puts("abcdefgh\n");
	ut_assertok(run_command("askenv t_ask3 4", 0));
	ut_assert_nextline("Please enter 't_ask3': abcdefgh");
	ut_assert_console_end();
	ut_asserteq_str("abcd", env_get("t_ask3"));

	/* a size of zero asks nothing and fails */
	ut_asserteq(1, run_command("askenv t_ask3 0", 0));
	ut_assert_console_end();
	ut_asserteq_str("abcd", env_get("t_ask3"));

	return 0;
}
CMD_TEST(cmd_test_askenv_size, UTF_CONSOLE);

/* Test that an empty value deletes the variable */
static int cmd_test_askenv_empty(struct unit_test_state *uts)
{
	ut_assertok(env_set("t_ask4", "something"));

	console_in_puts("\n");
	ut_assertok(run_command("askenv t_ask4", 0));
	ut_assert_nextline("Please enter 't_ask4': ");
	ut_assert_console_end();
	ut_assertnull(env_get("t_ask4"));

	/* deleting a variable which does not exist is not an error */
	console_in_puts("\n");
	ut_assertok(run_command("askenv t_ask4", 0));
	ut_assert_nextline("Please enter 't_ask4': ");
	ut_assert_console_end();
	ut_assertnull(env_get("t_ask4"));

	return 0;
}
CMD_TEST(cmd_test_askenv_empty, UTF_CONSOLE);

/* Test that Ctrl-C leaves the variable alone */
static int cmd_test_askenv_cancel(struct unit_test_state *uts)
{
	ut_assertok(env_set("t_ask5", "keep me"));

	console_in_puts("\x03");
	ut_asserteq(1, run_command("askenv t_ask5", 0));
	ut_assert_nextlinen("Please enter 't_ask5': ");
	ut_assert_console_end();
	ut_asserteq_str("keep me", env_get("t_ask5"));

	return 0;
}
CMD_TEST(cmd_test_askenv_cancel, UTF_CONSOLE);

/* Test 'askenv' with a bad command line */
static int cmd_test_askenv_usage(struct unit_test_state *uts)
{
	/* the variable name is required */
	ut_asserteq(1, run_command("askenv", 0));
	ut_assert_nextline("askenv - get environment variables from stdin");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("askenv name [message] [size]");
	ut_assert_nextlinen("    - get environment variable 'name' from stdin");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_askenv_usage, UTF_CONSOLE);
