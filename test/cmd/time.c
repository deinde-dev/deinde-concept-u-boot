// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the time command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <time.h>
#include <vsprintf.h>
#include <linux/errno.h>
#include <test/cmd.h>
#include <test/ut.h>

/**
 * read_report() - Read the report the command writes and work out its value
 *
 * The report reads 'time: 1.234 seconds', with a '1 minutes,' part before the
 * seconds where the command takes a minute or more.
 *
 * @uts: Test state
 * @msp: Returns the time reported, in milliseconds
 * Return: 0 if OK, -ve on error
 */
static int read_report(struct unit_test_state *uts, ulong *msp)
{
	ulong mins = 0, secs, ms;
	char *end;

	ut_assert_nextlinen("time: ");
	secs = dectoul(uts->actual_str + strlen("time: "), &end);
	if (!strncmp(end, " minutes, ", strlen(" minutes, "))) {
		mins = secs;
		secs = dectoul(end + strlen(" minutes, "), &end);
	}
	ut_asserteq('.', *end);
	ms = dectoul(end + 1, &end);
	ut_asserteq_str(" seconds", end);
	*msp = (mins * 60 + secs) * 1000 + ms;

	return 0;
}

/* Test 'time' with a command which takes almost no time */
static int cmd_test_time_base(struct unit_test_state *uts)
{
	ulong start, elapsed, ms;

	start = get_timer(0);
	ut_assertok(run_command("time echo hello world", 0));
	elapsed = get_timer(start);

	/* the command runs, then the report follows after a blank line */
	ut_assert_nextline("hello world");
	ut_assert_nextline_empty();
	ut_assertok(read_report(uts, &ms));
	ut_assert_console_end();

	/* the report cannot be longer than the time the test itself measures */
	ut_assert(ms <= elapsed);

	return 0;
}
CMD_TEST(cmd_test_time_base, UTF_CONSOLE);

/* Test that the time reported is the time the command really takes */
static int cmd_test_time_sleep(struct unit_test_state *uts)
{
	ulong start, elapsed, ms;

	if (!IS_ENABLED(CONFIG_CMD_SLEEP))
		return -EAGAIN;

	start = get_timer(0);
	ut_assertok(run_command("time sleep 1", 0));
	elapsed = get_timer(start);

	ut_assert_nextline_empty();
	ut_assertok(read_report(uts, &ms));
	ut_assert_console_end();

	/* the sleep waits for at least the second it is asked for */
	ut_assert(ms >= 1000);
	ut_assert(ms <= elapsed);

	return 0;
}
CMD_TEST(cmd_test_time_sleep, UTF_CONSOLE);

/* Test that an option of the command being timed is left alone */
static int cmd_test_time_opt(struct unit_test_state *uts)
{
	ulong ms;

	/* 'echo -n' writes no newline, so the report ends the same line */
	ut_assertok(run_command("time echo -n hi", 0));
	ut_assert_nextline("hi");
	ut_assertok(read_report(uts, &ms));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_time_opt, UTF_CONSOLE);

/**
 * check_usage() - Check that the usage message of the command follows
 *
 * @uts: Test state
 * Return: 0 if OK, -ve on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("time - run commands and summarize execution time");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("time command [args...]");
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}

/* Test 'time' with no command to run */
static int cmd_test_time_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("time", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_time_usage, UTF_CONSOLE);

/* Test that an option of the time command itself is refused */
static int cmd_test_time_noopts(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("time -n echo hi", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_time_noopts, UTF_CONSOLE);

/* Test that the return value of the command which ran comes back */
static int cmd_test_time_fail(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_CMD_SLEEP))
		return -EAGAIN;

	/* 'sleep' with no delay shows its usage and fails */
	ut_asserteq(1, run_command("time sleep", 0));
	ut_assert_skip_to_linen("time: ");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_time_fail, UTF_CONSOLE);
