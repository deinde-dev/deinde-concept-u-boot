// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the cyclic command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <cyclic.h>
#include <malloc.h>
#include <asm/global_data.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/* Cycle time in ms and delay in us for the demo function */
#define DEMO_TIME_MS	1000
#define DEMO_DELAY_US	1

/* The usage line which is too long to write out where it is used */
#define HELP_DEMO \
	"cyclic demo <cycletime_ms> <delay_us> - register cyclic demo function"

/* What the demo sub-command reports, with the cycle time it is given */
#define DEMO_MSG \
	"Registered function \"cyclic_demo\" to be executed all %dms"

/*
 * The line 'cyclic list' shows for the demo function. The CPU time and the
 * frequency depend on how often the function has run by then, so only their
 * format can be checked
 */
#define DEMO_LINE \
	"function: cyclic_demo, cpu-time: [0-9]+ us, " \
	"frequency: [0-9]+\\.[0-9][0-9] times/s"

/**
 * check_usage() - Check the usage message the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("cyclic - Cyclic");
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline(HELP_DEMO);
		ut_assert_nextline("cyclic list - list cyclic functions");

		/* the help text ends in a newline, so a blank line follows */
		ut_assert_nextline_empty();
	}
	ut_assert_console_end();

	return 0;
}

/**
 * drop_demo() - Unregister the demo function the command registers
 *
 * Nothing in the command removes the demo function, so the test does it, to
 * leave the cyclic list as it found it. The command allocates a struct whose
 * first member is the cyclic, so the list entry is also what it allocated
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int drop_demo(struct unit_test_state *uts)
{
	struct cyclic_info *cyclic;

	ut_assert(!hlist_empty(&gd->cyclic_list));
	cyclic = hlist_entry(gd->cyclic_list.first, struct cyclic_info, list);
	ut_asserteq_str("cyclic_demo", cyclic->name);
	cyclic_unregister(cyclic);
	free(cyclic);

	return 0;
}

/* Test registering the demo function and the list which then shows it */
static int cmd_test_cyclic_base(struct unit_test_state *uts)
{
	ut_assertok(run_commandf("cyclic demo %d %d", DEMO_TIME_MS,
				 DEMO_DELAY_US));
	ut_assert_nextline(DEMO_MSG, DEMO_TIME_MS);
	ut_assert_console_end();

	/*
	 * The demo goes to the head of the list, so it is the first line. A
	 * board may have cyclic functions of its own, which follow it
	 */
	ut_assertok(run_command("cyclic list", 0));
	ut_assert_nextline_regex(DEMO_LINE);
	console_record_reset();

	ut_assertok(drop_demo(uts));

	return 0;
}
CMD_TEST(cmd_test_cyclic_base, UTF_CONSOLE);

/* Test that the list shows every registered function, newest first */
static int cmd_test_cyclic_list(struct unit_test_state *uts)
{
	/* each demo registers a function of its own, with its own cycle time */
	ut_assertok(run_command("cyclic demo 50 2", 0));
	ut_assert_nextline(DEMO_MSG, 50);

	ut_assertok(run_command("cyclic demo 60 2", 0));
	ut_assert_nextline(DEMO_MSG, 60);
	ut_assert_console_end();

	/* so the list has a line for each of them */
	ut_assertok(run_command("cyclic list", 0));
	ut_assert_nextline_regex(DEMO_LINE);
	ut_assert_nextline_regex(DEMO_LINE);
	console_record_reset();

	ut_assertok(drop_demo(uts));
	ut_assertok(drop_demo(uts));

	return 0;
}
CMD_TEST(cmd_test_cyclic_list, UTF_CONSOLE);

/* Test the arguments the command refuses */
static int cmd_test_cyclic_usage(struct unit_test_state *uts)
{
	/* the command does nothing without a sub-command */
	ut_asserteq(1, run_command("cyclic", 0));
	ut_assertok(check_usage(uts));

	/* nor with a sub-command which does not exist */
	ut_asserteq(1, run_command("cyclic bogus", 0));
	ut_assertok(check_usage(uts));

	/* demo needs both of its arguments, and takes no more */
	ut_asserteq(1, run_command("cyclic demo", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("cyclic demo 50", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("cyclic demo 50 2 3", 0));
	ut_assertok(check_usage(uts));

	/* list takes no arguments at all */
	ut_asserteq(1, run_command("cyclic list extra", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_cyclic_usage, UTF_CONSOLE);
