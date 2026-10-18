// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the cpu command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Number of CPUs in the sandbox devicetree */
#define CPU_COUNT	3

/* Description the sandbox CPU driver reports for every core */
#define CPU_DESC	"LEG Inc. SuperMegaUltraTurbo CPU No. 1"

/*
 * Detail for a core. The driver reports a frequency of 42^5 Hz, which
 * print_freq() shows as 130.70 MHz, and a feature word of 0x42424242, of
 * which only the MMU bit is one the command knows a name for. The ID is the
 * platform CPU ID, which no sandbox driver sets, rather than the sequence
 * number in the first column
 */
#define CPU_DETAIL	"\tID = 0, freq = 130.70 MHz: MMU"

/* The two long-help lines which are too long to write out where used */
#define HELP_RELEASE \
	"cpu release <core ID> <addr>\t- Release CPU <core ID> at <addr>"
#define HELP_CORE_ID \
	"            <core ID>: the sequence number in list subcommand outputs"

/**
 * check_cpu_line() - Check the line the command prints for one CPU
 *
 * @uts: Test state
 * @seq: Sequence number of the CPU, which is also its position in the list
 * Return: 0 if OK, other value on error
 */
static int check_cpu_line(struct unit_test_state *uts, int seq)
{
	char name[10];

	snprintf(name, sizeof(name), "cpu@%d", seq + 1);
	ut_assert_nextline("%3d: %-10s %s", seq, name, CPU_DESC);

	return 0;
}

/**
 * check_usage() - Check the usage message the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("cpu - display information about CPUs");
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("cpu list\t- list available CPUs");
		ut_assert_nextline("cpu detail\t- show CPU detail");
		ut_assert_nextline(HELP_RELEASE);
		ut_assert_nextline(HELP_CORE_ID);
	}
	ut_assert_console_end();

	return 0;
}

/* Test the list of CPUs the command shows */
static int cmd_test_cpu_base(struct unit_test_state *uts)
{
	int i;

	ut_assertok(run_command("cpu list", 0));
	for (i = 0; i < CPU_COUNT; i++)
		ut_assertok(check_cpu_line(uts, i));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_cpu_base, UTF_CONSOLE);

/* Test the extra line the detail sub-command adds for each CPU */
static int cmd_test_cpu_detail(struct unit_test_state *uts)
{
	int i;

	ut_assertok(run_command("cpu detail", 0));
	for (i = 0; i < CPU_COUNT; i++) {
		ut_assertok(check_cpu_line(uts, i));
		ut_assert_nextline(CPU_DETAIL);
	}
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_cpu_detail, UTF_CONSOLE);

/* Test releasing a core, which the sandbox driver accepts without comment */
static int cmd_test_cpu_release(struct unit_test_state *uts)
{
	ut_assertok(run_command("cpu release 0 1000", 0));
	ut_assert_console_end();

	/* the last core is the highest sequence number the list shows */
	ut_assertok(run_commandf("cpu release %d 1000", CPU_COUNT - 1));
	ut_assert_console_end();

	/* a core which does not exist fails, albeit silently */
	ut_asserteq(1, run_commandf("cpu release %d 1000", CPU_COUNT));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_cpu_release, UTF_CONSOLE);

/* Test the arguments the command refuses */
static int cmd_test_cpu_usage(struct unit_test_state *uts)
{
	/* the command does nothing without a sub-command */
	ut_asserteq(1, run_command("cpu", 0));
	ut_assertok(check_usage(uts));

	/* nor with a sub-command which does not exist */
	ut_asserteq(1, run_command("cpu bogus", 0));
	ut_assertok(check_usage(uts));

	/* release needs both of its arguments, and takes no more */
	ut_asserteq(1, run_command("cpu release 0", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("cpu release 0 1000 2000", 0));
	ut_assertok(check_usage(uts));

	/* list takes no arguments at all */
	ut_asserteq(1, run_command("cpu list extra", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_cpu_usage, UTF_CONSOLE);
