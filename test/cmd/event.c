// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the event command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <event.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Width of the function-address column, as event_show_spy_list() sets it */
#define ADDR_WIDTH	(int)(sizeof(ulong) * 2)

/**
 * check_usage() - Check the usage message the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("event - Events");
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("event list - list event spies");
	}
	ut_assert_console_end();

	return 0;
}

/* Test the spy list the command shows */
static int cmd_test_event_base(struct unit_test_state *uts)
{
	struct evspy_info *start = ll_entry_start(struct evspy_info,
						  evspy_info);
	const int n_ents = ll_entry_count(struct evspy_info, evspy_info);
	struct evspy_info *spy;

	ut_assertok(run_command("event list", 0));
	ut_assert_nextline("Seq  %-24s  %*s  %s", "Type", ADDR_WIDTH,
			   "Function", "ID");

	/*
	 * The command walks the linker list in order, so the sequence numbers
	 * it shows are the positions in that list
	 */
	for (spy = start; spy != start + n_ents; spy++) {
		ut_assert_nextline("%3x  %-3x %-20s  %*p  %s",
				   (uint)(spy - start), spy->type,
				   event_type_name(spy->type), ADDR_WIDTH,
				   spy->func, event_spy_id(spy));
	}
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_event_base, UTF_CONSOLE);

/* Test the arguments the command refuses */
static int cmd_test_event_usage(struct unit_test_state *uts)
{
	/* the command does nothing without a sub-command */
	ut_asserteq(1, run_command("event", 0));
	ut_assertok(check_usage(uts));

	/* nor with a sub-command which does not exist */
	ut_asserteq(1, run_command("event bogus", 0));
	ut_assertok(check_usage(uts));

	/* list takes no arguments at all */
	ut_asserteq(1, run_command("event list extra", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_event_usage, UTF_CONSOLE);
