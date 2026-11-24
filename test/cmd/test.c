// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the test, true and false commands
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Test 'test' with the argument counts it treats specially */
static int cmd_test_test_base(struct unit_test_state *uts)
{
	/* an empty expression is false */
	ut_asserteq(1, run_command("test", 0));

	/* a single argument is true only if it is not empty */
	ut_asserteq(1, run_command("test ''", 0));
	ut_assertok(run_command("test abc", 0));

	/* '-n' on its own is read as 'test -n ""', so it is false */
	ut_asserteq(1, run_command("test -n", 0));

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_test_base, UTF_CONSOLE);

/* Test the string operators of 'test' */
static int cmd_test_test_string(struct unit_test_state *uts)
{
	ut_assertok(run_command("test -n abc", 0));
	ut_asserteq(1, run_command("test -n ''", 0));
	ut_assertok(run_command("test -z ''", 0));
	ut_asserteq(1, run_command("test -z abc", 0));

	ut_assertok(run_command("test abc = abc", 0));
	ut_asserteq(1, run_command("test abc = abd", 0));
	ut_assertok(run_command("test abc != abd", 0));
	ut_asserteq(1, run_command("test abc != abc", 0));

	/* hush passes '<' and '>' through, since it has no redirection */
	ut_assertok(run_command("test abc < abd", 0));
	ut_asserteq(1, run_command("test abd < abc", 0));
	ut_assertok(run_command("test abd > abc", 0));
	ut_asserteq(1, run_command("test abc > abd", 0));

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_test_string, UTF_CONSOLE);

/* Test the number operators of 'test' */
static int cmd_test_test_number(struct unit_test_state *uts)
{
	ut_assertok(run_command("test 2 -eq 2", 0));
	ut_asserteq(1, run_command("test 2 -eq 3", 0));
	ut_assertok(run_command("test 2 -ne 3", 0));
	ut_assertok(run_command("test 2 -lt 3", 0));
	ut_asserteq(1, run_command("test 3 -lt 3", 0));
	ut_assertok(run_command("test 3 -le 3", 0));
	ut_assertok(run_command("test 3 -gt 2", 0));
	ut_asserteq(1, run_command("test 3 -gt 3", 0));
	ut_assertok(run_command("test 3 -ge 3", 0));

	/* the operands go through simple_strtol(), so 0x means hex */
	ut_assertok(run_command("test 0x10 -eq 16", 0));
	ut_assertok(run_command("test 010 -eq 8", 0));

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_test_number, UTF_CONSOLE);

/* Test the '!', '-o' and '-a' operators of 'test' */
static int cmd_test_test_logic(struct unit_test_state *uts)
{
	ut_assertok(run_command("test ! abc = abd", 0));
	ut_asserteq(1, run_command("test ! abc = abc", 0));
	ut_asserteq(1, run_command("test ! ! abc = abd", 0));

	ut_assertok(run_command("test abc = abc -o abc = abd", 0));
	ut_asserteq(1, run_command("test abc = abd -o abd = abc", 0));

	ut_assertok(run_command("test abc = abc -a abd = abd", 0));
	ut_asserteq(1, run_command("test abc = abc -a abc = abd", 0));

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_test_logic, UTF_CONSOLE);

/*
 * Test an expression 'test' cannot parse
 *
 * The scan stops at an argument which is not an operator, or at an operator
 * with too few operands, and the command reports success in both cases
 */
static int cmd_test_test_unparsed(struct unit_test_state *uts)
{
	ut_assertok(run_command("test abc -q abd", 0));
	ut_assertok(run_command("test abc =", 0));

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_test_unparsed, UTF_CONSOLE);

/* Test the '[' alias, which needs a closing ']' */
static int cmd_test_test_lbracket(struct unit_test_state *uts)
{
	ut_assertok(run_command("[ abc = abc ]", 0));
	ut_asserteq(1, run_command("[ abc = abd ]", 0));

	/* the ']' must be an argument of its own */
	ut_asserteq(1, run_command("[ abc = abc", 0));
	ut_assert_nextline("[: missing terminating ]");
	ut_asserteq(1, run_command("[ abc = abc]", 0));
	ut_assert_nextline("[: missing terminating ]");

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_test_lbracket, UTF_CONSOLE);

/* Test the 'true' command */
static int cmd_test_true_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("true", 0));

	/* an ordinary argument is ignored */
	ut_assertok(run_command("true extra", 0));

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_true_base, UTF_CONSOLE);

/* Test that 'true' refuses an option, since it has none */
static int cmd_test_true_opt(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("true -n", 0));
	ut_assert_nextline("true - do nothing, successfully");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("true - No additional help available.");
	}
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_true_opt, UTF_CONSOLE);

/* Test the 'false' command */
static int cmd_test_false_base(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("false", 0));

	/* an ordinary argument is ignored */
	ut_asserteq(1, run_command("false extra", 0));

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_false_base, UTF_CONSOLE);

/* Test that 'false' refuses an option, since it has none */
static int cmd_test_false_opt(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("false -n", 0));
	ut_assert_nextline("false - do nothing, unsuccessfully");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("false - No additional help available.");
	}
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_false_opt, UTF_CONSOLE);
