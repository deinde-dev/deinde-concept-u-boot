// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for bootstage command
 *
 * Copyright 2025 Canonical Ltd
 */

#include <bootstage.h>
#include <command.h>
#include <errno.h>
#include <asm/global_data.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Where the stash is written and read back. A board with no default load
 * address leaves CONFIG_SYS_LOAD_ADDR at 0 and qemu-arm-sbsa has no memory
 * below 4GB, so the test checks this against the start of memory
 */
#define STASH_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x9000)

/* Room for the stash, which is a header followed by one record per mark */
#define STASH_SIZE	0x4000

static int cmd_test_bootstage_base(struct unit_test_state *uts)
{
	uint count;

	/* Get the current record count */
	count = bootstage_get_rec_count();
	ut_assert(count > 0);

	/* Test the bootstage report command runs successfully */
	ut_assertok(run_command("bootstage report", 0));

	/* Verify the report contains expected headers and stages */
	ut_assert_nextline("Timer summary in microseconds (%u records):",
			   count);
	ut_assert_nextline("       Mark    Elapsed  Stage");
	ut_assert_nextline("          0          0  reset");
	ut_assert_skip_to_line("Accumulated time:");

	return 0;
}
CMD_TEST(cmd_test_bootstage_base, UTF_CONSOLE);

static int cmd_test_bootstage_save_restore(struct unit_test_state *uts)
{
	uint count;

	if (!IS_ENABLED(CONFIG_BOOTSTAGE_SAVE))
		return -EAGAIN;

	count = bootstage_get_rec_count();
	ut_assert(count > 0);

	/* Save the current count */
	ut_assertok(run_command("bootstage save", 0));
	ut_assert_console_end();

	/* Add a new record and check the count grows by one */
	bootstage_mark_name(BOOTSTAGE_ID_USER + 60, "test_save_restore");
	ut_asserteq(count + 1, bootstage_get_rec_count());

	/* Restore should bring the count back */
	ut_assertok(run_command("bootstage restore", 0));
	ut_assert_console_end();
	ut_asserteq(count, bootstage_get_rec_count());

	return 0;
}
CMD_TEST(cmd_test_bootstage_save_restore, UTF_CONSOLE);

/* Test that a stash can be written to memory and read back from it */
static int cmd_test_bootstage_stash(struct unit_test_state *uts)
{
	uint count;

	if (!IS_ENABLED(CONFIG_BOOTSTAGE_STASH))
		return -EAGAIN;
	if (gd->ram_base > STASH_ADDR)
		return -EAGAIN;

	count = bootstage_get_rec_count();
	ut_assert(count > 0);

	/* Both sub-commands are silent when they work */
	ut_assertok(run_commandf("bootstage stash %lx %x", STASH_ADDR,
				 STASH_SIZE));
	ut_assert_console_end();

	/* Unstashing adds the stashed records to the ones already there */
	ut_assertok(run_commandf("bootstage unstash %lx %x", STASH_ADDR,
				 STASH_SIZE));
	ut_assert_console_end();
	ut_asserteq(count * 2, bootstage_get_rec_count());

	/* Put the table back, since every later test shares it */
	bootstage_set_rec_count(count);

	return 0;
}
CMD_TEST(cmd_test_bootstage_stash, UTF_CONSOLE);

/**
 * check_usage() - Check the usage message a command line the command refuses
 *	shows
 *
 * The lines after the first are there only where the long help is, and which
 * sub-commands they name depends on the configuration, so this checks as far
 * as the first of them and drops the rest
 *
 * @uts: Test state
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("bootstage - Boot stage command");
	ut_assert_nextline_empty();

	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("bootstage  - check boot progress and timing");
		console_record_reset();
	} else {
		ut_assert_console_end();
	}

	return 0;
}

/* Test the error paths */
static int cmd_test_bootstage_err(struct unit_test_state *uts)
{
	/* A command line with no sub-command at all */
	ut_asserteq(1, run_command("bootstage", 0));
	ut_assertok(check_usage(uts));

	/* A sub-command the command does not have */
	ut_asserteq(1, run_command("bootstage nonsense", 0));
	ut_assertok(check_usage(uts));

	/* An option, which the command does not take */
	ut_asserteq(1, run_command("bootstage -v", 0));
	ut_assertok(check_usage(uts));

	/* An argument a sub-command has no use for */
	if (IS_ENABLED(CONFIG_BOOTSTAGE_SAVE)) {
		ut_asserteq(1, run_command("bootstage save extra", 0));
		ut_assertok(check_usage(uts));
	}

	if (IS_ENABLED(CONFIG_BOOTSTAGE_STASH)) {
		/* An address which is not a number */
		ut_asserteq(1, run_command("bootstage stash xyz", 0));
		ut_assertok(check_usage(uts));

		/* A size which is not a number */
		ut_asserteq(1, run_command("bootstage unstash 100 xyz", 0));
		ut_assertok(check_usage(uts));

		/*
		 * An all-ones address means no stash area, which the command
		 * reports rather than writing there
		 */
		ut_asserteq(1, run_commandf("bootstage stash %lx", -1UL));
		ut_assert_nextline("No bootstage stash area defined");
		ut_assert_console_end();
	}

	return 0;
}
CMD_TEST(cmd_test_bootstage_err, UTF_CONSOLE);

/* Test that the dispatcher reports the repeatability of its sub-command */
static int cmd_test_bootstage_repeat(struct unit_test_state *uts)
{
	char *const report_args[] = { "bootstage", "report", NULL };
	char *const save_args[] = { "bootstage", "save", NULL };
	int repeatable;

	/* 'report' may be repeated by pressing Enter */
	repeatable = -1;
	ut_assertok(cmd_process(0, 2, report_args, &repeatable, NULL));
	ut_asserteq(1, repeatable);
	console_record_reset();

	if (!IS_ENABLED(CONFIG_BOOTSTAGE_SAVE))
		return 0;

	/*
	 * 'save' may not, and the dispatcher has to say so, since the command
	 * itself is marked repeatable
	 */
	repeatable = -1;
	ut_assertok(cmd_process(0, 2, save_args, &repeatable, NULL));
	ut_asserteq(0, repeatable);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_bootstage_repeat, UTF_CONSOLE);
