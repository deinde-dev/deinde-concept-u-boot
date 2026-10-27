// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the setenv command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <env.h>
#include <errno.h>
#include <test/cmd.h>
#include <test/ut.h>

/* the message shown when a variable name holds an '=' */
#define ILLEGAL_CHAR	"## Error: illegal character '=' in variable name"

/* Test 'setenv' setting, changing and deleting a variable */
static int cmd_test_setenv_base(struct unit_test_state *uts)
{
	/* with a writeable list only the variables in it can be set */
	if (IS_ENABLED(CONFIG_ENV_WRITEABLE_LIST))
		return -EAGAIN;

	/* a variable which does not exist is created */
	ut_assertok(run_command("setenv ut_set wilma", 0));
	ut_asserteq_str("wilma", env_get("ut_set"));

	/* setting it again replaces the value */
	ut_assertok(run_command("setenv ut_set betty", 0));
	ut_asserteq_str("betty", env_get("ut_set"));

	/* the arguments after the name are joined with a single space */
	ut_assertok(run_command("setenv ut_set one   two three", 0));
	ut_asserteq_str("one two three", env_get("ut_set"));

	/* with no value the variable is deleted */
	ut_assertok(run_command("setenv ut_set", 0));
	ut_assertnull(env_get("ut_set"));

	/* deleting a variable which is not there succeeds too */
	ut_assertok(run_command("setenv ut_set", 0));
	ut_assertnull(env_get("ut_set"));

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_setenv_base, UTF_CONSOLE);

/* Test that 'setenv -f' ignores the access flags of a variable */
static int cmd_test_setenv_force(struct unit_test_state *uts)
{
	/* there is nothing to force when forced access is blocked */
	if (IS_ENABLED(CONFIG_ENV_ACCESS_IGNORE_FORCE) ||
	    IS_ENABLED(CONFIG_ENV_WRITEABLE_LIST))
		return -EAGAIN;

	/*
	 * Create the variable before giving it a flag. The '.flags' list is
	 * read when a variable is created and again when the list itself
	 * changes, so this order is the one which attaches the flag
	 */
	ut_assertok(run_command("setenv ut_set wilma", 0));
	ut_assertok(run_command("setenv .flags ut_set:so", 0));

	/* a write-once variable keeps the value it has */
	ut_asserteq(1, run_command("setenv ut_set betty", 0));
	ut_assert_nextline("## Error: Can't overwrite \"ut_set\"");
	ut_assert_nextline("## Error inserting \"ut_set\" variable, errno=1");
	ut_assert_console_end();
	ut_asserteq_str("wilma", env_get("ut_set"));

	/* with -f the new value goes in */
	ut_assertok(run_command("setenv -f ut_set betty", 0));
	ut_asserteq_str("betty", env_get("ut_set"));
	ut_assert_console_end();

	/* dropping the list makes the variable writeable again */
	ut_assertok(run_command("setenv .flags", 0));
	ut_assertok(run_command("setenv ut_set", 0));
	ut_assertnull(env_get("ut_set"));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_setenv_force, UTF_CONSOLE);

/* Test 'setenv' rejecting a bad command line */
static int cmd_test_setenv_error(struct unit_test_state *uts)
{
	/* the name is required */
	ut_asserteq(1, run_command("setenv", 0));
	ut_assert_nextlinen("setenv - set environment variables");
	ut_assert_nextline_empty();
	ut_assert_nextlinen("Usage:");
	ut_assert_skip_to_linen("    - [forcibly] delete environment");
	ut_assert_console_end();

	/* '=' separates a name from its value, so a name cannot hold one */
	ut_asserteq(1, run_command("setenv ut=set wilma", 0));
	ut_assert_nextline(ILLEGAL_CHAR " \"ut=set\"");
	ut_assert_console_end();
	ut_assertnull(env_get("ut=set"));

	/* an unknown option is refused and nothing is set */
	ut_asserteq(1, run_command("setenv -x ut_set wilma", 0));
	ut_assert_nextlinen("setenv - set environment variables");
	ut_assert_skip_to_linen("    - [forcibly] delete environment");
	ut_assert_console_end();
	ut_assertnull(env_get("ut_set"));

	return 0;
}
CMD_TEST(cmd_test_setenv_error, UTF_CONSOLE);
