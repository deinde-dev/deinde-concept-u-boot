// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the log command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <log.h>
#include <vsprintf.h>
#include <asm/global_data.h>
#include <linux/errno.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/* a record which 'log format' can show every field of */
#define LOG_REC	"log rec arch notice file.c 123 func msg"

/**
 * check_usage() - Check the usage message the command shows
 *
 * The 'Usage:' part comes from cmd_usage(), which only shows it with
 * CONFIG_SYS_LONGHELP. The help text runs to 30 lines, so this checks the
 * first few and drops the rest.
 *
 * @uts: Test state
 * Return: 0 if OK, -ve on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("log - log system");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("log level [<level>] - get/set log level");
		ut_assert_nextline("log categories - list log categories");
		ut_assert_nextline("log drivers - list log drivers");

		/* the rest of the help text, one line per option */
		console_record_reset();
	}
	ut_assert_console_end();

	return 0;
}

/**
 * drop_filters() - Remove every filter from the console driver
 *
 * A test which stops part-way through would otherwise leave its filters in
 * place, where they decide what the rest of the suite is allowed to emit.
 * This makes no assertion, so that the test's own result survives.
 */
static void drop_filters(void)
{
	run_command("log filter-remove -a", 0);
	console_record_reset();
}

/**
 * check_levels() - Check 'log level' listing and setting the default level
 *
 * @uts: Test state
 * Return: 0 if OK, -ve on error
 */
static int check_levels(struct unit_test_state *uts)
{
	/* set the level by name, then look for its marker in the list */
	ut_assertok(run_command("log level warning", 0));
	ut_assert_console_end();

	ut_assertok(run_command("log level", 0));
	ut_assert_nextline("EMERG");
	ut_assert_nextline("ALERT");
	ut_assert_nextline("CRIT");
	ut_assert_nextline("ERR");
	ut_assert_nextline("WARNING (default)");
	ut_assert_nextline("NOTICE");

	/* the list stops at CONFIG_LOG_MAX_LEVEL */
	console_record_reset();

	/* and by number, which is the position in that list */
	ut_assertok(run_command("log level 3", 0));
	ut_assert_console_end();

	ut_assertok(run_command("log level", 0));
	ut_assert_nextline("EMERG");
	ut_assert_nextline("ALERT");
	ut_assert_nextline("CRIT");
	ut_assert_nextline("ERR (default)");
	console_record_reset();

	return 0;
}

/* Test 'log level' */
static int cmd_test_log_base(struct unit_test_state *uts)
{
	enum log_level_t old_level = gd->default_log_level;
	int ret;

	/* the test asks for WARNING, so the build must have it */
	if (_LOG_MAX_LEVEL < LOGL_WARNING)
		return -EAGAIN;

	ret = check_levels(uts);
	gd->default_log_level = old_level;

	return ret;
}
CMD_TEST(cmd_test_log_base, UTF_CONSOLE);

/* Test 'log level' with a level it cannot use */
static int cmd_test_log_level_err(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("log level bogus", 0));
	ut_assert_nextline("Unknown log level \"bogus\"");
	ut_assert_console_end();

	ut_asserteq(1, run_command("log level 99", 0));
	ut_assert_nextline("Only log levels <= %d are supported",
			   _LOG_MAX_LEVEL);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_log_level_err, UTF_CONSOLE);

/* Test 'log categories' and 'log drivers' */
static int cmd_test_log_lists(struct unit_test_state *uts)
{
	ut_assertok(run_command("log categories", 0));

	/*
	 * the list starts with the driver-model uclasses, which depend on the
	 * build, and ends with the categories the logging system declares
	 */
	ut_assert_skip_to_line("none");
	ut_assert_nextline("arch");
	ut_assert_nextline("board");
	ut_assert_nextline("core");
	console_record_reset();

	ut_assertok(run_command("log drivers", 0));

	/* the drivers come out in linker-list order, so console is first */
	ut_assert_nextline("console");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_log_lists, UTF_CONSOLE);

/**
 * check_format_err() - Check 'log format' with a letter it does not know
 *
 * @uts: Test state
 * Return: 0 if OK, -ve on error
 */
static int check_format_err(struct unit_test_state *uts)
{
	ut_assertok(run_command("log format lm", 0));
	ut_assert_console_end();

	ut_asserteq(1, run_command("log format mq", 0));
	ut_assert_nextline("Invalid log char 'q'");
	ut_assert_console_end();

	/* the format the command refused is not applied in part */
	ut_assertok(run_command("log format", 0));
	ut_assert_nextline("Log format: lm");
	ut_assert_console_end();

	return 0;
}

/* Test 'log format' with a letter it does not know */
static int cmd_test_log_format_err(struct unit_test_state *uts)
{
	int old_fmt = gd->log_fmt;
	int ret;

	ret = check_format_err(uts);
	gd->log_fmt = old_fmt;

	return ret;
}
CMD_TEST(cmd_test_log_format_err, UTF_CONSOLE);

/**
 * check_rec() - Check the record 'log rec' produces
 *
 * @uts: Test state
 * Return: 0 if OK, -ve on error
 */
static int check_rec(struct unit_test_state *uts)
{
	char cmd[32];
	ulong num;

	ut_assertok(run_command("log format all", 0));
	ut_assertok(run_command("log level notice", 0));
	ut_assert_console_end();

	ut_assertok(run_command(LOG_REC, 0));
	ut_assert_nextline("NOTICE.arch,file.c:123-%*s() msg",
			   CONFIG_LOGF_FUNC_PAD, "func");
	ut_assert_console_end();

	/* a record less important than the default level is quietly dropped */
	ut_assertok(run_command("log rec arch info file.c 124 func quiet", 0));
	ut_assert_console_end();

	/* an unknown category is reported as 'none' rather than refused */
	ut_assertok(run_command("log rec zzz notice file.c 125 func cat", 0));
	ut_assert_nextline("NOTICE.none,file.c:125-%*s() cat",
			   CONFIG_LOGF_FUNC_PAD, "func");
	ut_assert_console_end();

	/*
	 * IO is the least important level and the last valid one. Let it
	 * through with a filter of this test's own rather than by raising the
	 * default level, which would bring out every other log_io() in U-Boot
	 */
	ut_assertok(run_command("log filter-add -p -A -l io -f file.c", 0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_assertok(strict_strtoul(uts->actual_str, 10, &num));
	ut_assert_console_end();

	ut_assertok(run_command("log rec arch io file.c 126 func io", 0));
	ut_assert_nextline("IO.arch,file.c:126-%*s() io",
			   CONFIG_LOGF_FUNC_PAD, "func");
	ut_assert_console_end();

	snprintf(cmd, sizeof(cmd), "log filter-remove %lu", num);
	ut_assertok(run_command(cmd, 0));
	ut_assert_console_end();

	return 0;
}

/* Test 'log rec' */
static int cmd_test_log_rec(struct unit_test_state *uts)
{
	enum log_level_t old_level = gd->default_log_level;
	int old_fmt = gd->log_fmt;
	int ret;

	if (_LOG_MAX_LEVEL < LOGL_DEBUG_IO)
		return -EAGAIN;

	ret = check_rec(uts);
	gd->default_log_level = old_level;
	gd->log_fmt = old_fmt;
	drop_filters();

	return ret;
}
CMD_TEST(cmd_test_log_rec, UTF_CONSOLE);

/* Test 'log rec' with arguments it cannot use */
static int cmd_test_log_rec_err(struct unit_test_state *uts)
{
	/* the message is required */
	ut_asserteq(1, run_command("log rec arch notice file.c 1 func", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("log rec arch bogus file.c 1 func msg", 0));
	ut_assert_nextline("Invalid log level 'bogus'");
	ut_assertok(check_usage(uts));

	/* one past the last valid level */
	ut_asserteq(1, run_command("log rec arch 10 file.c 1 func msg", 0));
	ut_assert_nextline("Invalid log level 10");
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_log_rec_err, UTF_CONSOLE);

/**
 * check_filters() - Check adding, listing and removing filters
 *
 * @uts: Test state
 * Return: 0 if OK, -ve on error
 */
static int check_filters(struct unit_test_state *uts)
{
	char cmd[32];
	ulong allow, deny;

	/* nothing attaches a filter on the way up, so the list starts empty */
	ut_assertok(run_command("log filter-list", 0));
	ut_assert_console_end();

	ut_assertok(run_command("log filter-add -p -D -c spi -c mmc", 0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_assertok(strict_strtoul(uts->actual_str, 10, &deny));
	ut_assert_console_end();

	ut_assertok(run_command("log filter-add -p -A -L warning -f f.c -F fn",
				0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_assertok(strict_strtoul(uts->actual_str, 10, &allow));
	ut_assert_console_end();

	/*
	 * a deny filter goes to the head of the list and an allow one to the
	 * tail, whatever order they are added in. A filter with no level of
	 * its own matches up to the least important level
	 */
	ut_assertok(run_command("log filter-list", 0));
	ut_assert_nextline("%-3ld: DENY <= IO", deny);
	ut_assert_nextline("     Categories: spi mmc");
	ut_assert_nextline("%-3ld: ALLOW >= WARNING", allow);
	ut_assert_nextline("     Files: f.c");
	ut_assert_nextline("     Functions: fn");
	ut_assert_console_end();

	/* removing one by number leaves the other */
	snprintf(cmd, sizeof(cmd), "log filter-remove %lu", deny);
	ut_assertok(run_command(cmd, 0));
	ut_assert_console_end();

	ut_assertok(run_command("log filter-list", 0));
	ut_assert_nextline("%-3ld: ALLOW >= WARNING", allow);
	ut_assert_nextline("     Files: f.c");
	ut_assert_nextline("     Functions: fn");
	ut_assert_console_end();

	ut_assertok(run_command("log filter-remove -a", 0));
	ut_assert_console_end();

	ut_assertok(run_command("log filter-list", 0));
	ut_assert_console_end();

	return 0;
}

/* Test 'log filter-add', 'log filter-list' and 'log filter-remove' */
static int cmd_test_log_filter(struct unit_test_state *uts)
{
	int ret;

	ret = check_filters(uts);
	drop_filters();

	return ret;
}
CMD_TEST(cmd_test_log_filter, UTF_CONSOLE);

/* Test the filter sub-commands with arguments they cannot use */
static int cmd_test_log_filter_err(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("log filter-list -d bogus", 0));
	ut_assert_nextline("Could not find log device for \"bogus\"");
	ut_assert_console_end();

	ut_asserteq(1, run_command("log filter-remove -a -d bogus", 0));
	ut_assert_nextline("Could not find log device for \"bogus\"");
	ut_assert_console_end();

	ut_asserteq(1, run_command("log filter-remove abc", 0));
	ut_assert_nextline("Invalid filter number \"abc\"");
	ut_assert_console_end();

	/* a number which names no filter */
	ut_asserteq(1, run_command("log filter-remove 65535", 0));
	ut_assert_nextline("Could not remove filter (err = %d)", -ENOENT);
	ut_assert_console_end();

	ut_asserteq(1, run_command("log filter-add -c bogus", 0));
	ut_assert_nextline("Unknown category \"bogus\"");
	ut_assert_console_end();

	/* -A and -D are mutually exclusive, as are -l and -L */
	ut_asserteq(1, run_command("log filter-add -A -D", 0));
	ut_assert_nextline("Allow or deny set twice");
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("log filter-add -l info -L info", 0));
	ut_assert_nextline("Log level set twice");
	ut_assertok(check_usage(uts));

	/* the list ends up as it started */
	ut_assertok(run_command("log filter-list", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_log_filter_err, UTF_CONSOLE);

/* Test the usage message */
static int cmd_test_log_usage(struct unit_test_state *uts)
{
	/* no sub-command */
	ut_asserteq(1, run_command("log", 0));
	ut_assertok(check_usage(uts));

	/* a sub-command which does not exist */
	ut_asserteq(1, run_command("log bogus", 0));
	ut_assertok(check_usage(uts));

	/* an option the command itself does not take */
	ut_asserteq(1, run_command("log -x", 0));
	ut_assertok(check_usage(uts));

	/* an argument a sub-command does not take */
	ut_asserteq(1, run_command("log categories extra", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_log_usage, UTF_CONSOLE);
