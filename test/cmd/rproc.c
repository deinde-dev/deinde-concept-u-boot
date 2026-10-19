// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the rproc command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <dm.h>
#include <errno.h>
#include <test/cmd.h>
#include <test/ut.h>

/*
 * One of the three processors sandbox binds comes from platform data, so the
 * tests need a tree of their own with that data scanned in as well as the
 * devicetree. Without UTF_DM they would run against processors which an
 * earlier test has initialised, and 'rproc init' only works once
 */
#define RPROC_TEST_FLAGS (UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA | UTF_SCAN_FDT)

/* The processor bound from platform data, which has no devicetree node */
#define RPROC_LEGACY	2

/* The two processors with a devicetree node */
#define RPROC_DEV1	0
#define RPROC_DEV2	1

/* Processor which is not there, since sandbox binds three */
#define RPROC_MISSING	9

/*
 * Address and size for 'rproc load'. The sandbox processor reads neither, so
 * nothing is copied and the values only have to reach the message
 */
#define RPROC_ADDR	0x1000UL
#define RPROC_SIZE	0x100UL

/*
 * How a sandbox processor reports its memory. The uclass takes a processor
 * with no 'remoteproc-internal-memory-mapped' property to be mapped as well
 */
#define RPROC_TYPE	"internal memory mapped"

/* Operations every sandbox processor provides, with the trailing space */
#define RPROC_OPS	"load start stop reset is_running ping "

/* The start of the message 'rproc load' prints */
#define RPROC_LOAD_MSG	"Load Remote Processor %d with data@addr="

/* The last line of the help text, which ends with a newline of its own */
#define RPROC_USAGE_LAST \
	"\tping <id>\t- Ping the remote processor for communication"

/**
 * check_list_line() - Check one line of the 'rproc list' output
 *
 * @uts: Test state
 * @id: ID the processor is expected to have
 * @name: Name the processor is expected to report
 * Return: 0 if OK, -ve on error
 */
static int check_list_line(struct unit_test_state *uts, int id,
			   const char *name)
{
	ut_assert_nextline("%d - Name:'%s' type:'%s' supports: %s", id, name,
			   RPROC_TYPE, RPROC_OPS);

	return 0;
}

/**
 * check_load_line() - Check the message from 'rproc load'
 *
 * @uts: Test state
 * @id: ID the load was asked for
 * @result: How the message ends, including the space before it
 * Return: 0 if OK, -ve on error
 */
static int check_load_line(struct unit_test_state *uts, int id,
			   const char *result)
{
	ut_assert_nextline(RPROC_LOAD_MSG "0x%08lx %lu bytes:%s", id,
			   RPROC_ADDR, RPROC_SIZE, result);

	return 0;
}

/* Test 'rproc list' and 'rproc init' */
static int cmd_test_rproc_base(struct unit_test_state *uts)
{
	/* nothing is probed to start with, so there is nothing to list */
	ut_assertok(run_command("rproc list", 0));
	ut_assert_console_end();

	ut_assertok(run_command("rproc init", 0));
	ut_assert_console_end();

	/* the processors are listed in the order they are bound */
	ut_assertok(run_command("rproc list", 0));
	ut_assertok(check_list_line(uts, RPROC_LEGACY, "proc_3_legacy"));
	ut_assertok(check_list_line(uts, RPROC_DEV1, "remoteproc-test-dev1"));
	ut_assertok(check_list_line(uts, RPROC_DEV2, "remoteproc-test-dev2"));
	ut_assert_console_end();

	/* a second init refuses, every processor being initialised already */
	ut_asserteq(1, run_command("rproc init", 0));
	ut_assert_nextline("\tRemote Processors are already initialized");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_rproc_base, RPROC_TEST_FLAGS);

/* Test 'rproc init <id>', which initialises one processor */
static int cmd_test_rproc_init_id(struct unit_test_state *uts)
{
	ut_assertok(run_commandf("rproc init %d", RPROC_DEV2));
	ut_assert_console_end();

	ut_assertok(run_command("rproc list", 0));
	ut_assertok(check_list_line(uts, RPROC_DEV2, "remoteproc-test-dev2"));
	ut_assert_console_end();

	/* the other two are still waiting, so init does the rest */
	ut_assertok(run_command("rproc init", 0));
	ut_assert_console_end();

	ut_assertok(run_command("rproc list", 0));
	ut_assertok(check_list_line(uts, RPROC_LEGACY, "proc_3_legacy"));
	ut_assertok(check_list_line(uts, RPROC_DEV1, "remoteproc-test-dev1"));
	ut_assertok(check_list_line(uts, RPROC_DEV2, "remoteproc-test-dev2"));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_rproc_init_id, RPROC_TEST_FLAGS);

/* Test loading a processor, starting it, pinging it and stopping it again */
static int cmd_test_rproc_ops(struct unit_test_state *uts)
{
	ut_assertok(run_command("rproc init", 0));
	ut_assert_console_end();

	/* a processor which has only been initialised is not running */
	ut_assertok(run_commandf("rproc is_running %d", RPROC_DEV1));
	ut_assert_nextline("Remote processor is NOT Running");
	ut_assert_console_end();

	/* so it has nothing to say */
	ut_asserteq(1, run_commandf("rproc ping %d", RPROC_DEV1));
	ut_assert_nextline("Operation Failed with error (%d)", -EINVAL);
	ut_assert_console_end();

	/* and it cannot start until it has an image */
	ut_asserteq(1, run_commandf("rproc start %d", RPROC_DEV1));
	ut_assert_nextline("Operation Failed with error (%d)", -EINVAL);
	ut_assert_console_end();

	ut_assertok(run_commandf("rproc load %d %lx %lx", RPROC_DEV1,
				 RPROC_ADDR, RPROC_SIZE));
	ut_assertok(check_load_line(uts, RPROC_DEV1, " Success!"));
	ut_assert_console_end();

	ut_assertok(run_commandf("rproc start %d", RPROC_DEV1));
	ut_assert_console_end();

	ut_assertok(run_commandf("rproc is_running %d", RPROC_DEV1));
	ut_assert_nextline("Remote processor is Running");
	ut_assert_console_end();

	ut_assertok(run_commandf("rproc ping %d", RPROC_DEV1));
	ut_assert_nextline("Remote processor responds 'Pong'");
	ut_assert_console_end();

	ut_assertok(run_commandf("rproc stop %d", RPROC_DEV1));
	ut_assert_console_end();

	ut_assertok(run_commandf("rproc reset %d", RPROC_DEV1));
	ut_assert_console_end();

	/* with the image gone, a start is refused again */
	ut_asserteq(1, run_commandf("rproc start %d", RPROC_DEV1));
	ut_assert_nextline("Operation Failed with error (%d)", -EINVAL);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_rproc_ops, RPROC_TEST_FLAGS);

/* Test the operations on a processor which is not there */
static int cmd_test_rproc_missing(struct unit_test_state *uts)
{
	ut_asserteq(1, run_commandf("rproc init %d", RPROC_MISSING));
	ut_assert_nextline("Remote Processor %d failed to be initialized",
			   RPROC_MISSING);
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("rproc load %d %lx %lx", RPROC_MISSING,
				    RPROC_ADDR, RPROC_SIZE));
	ut_assertok(check_load_line(uts, RPROC_MISSING, " Failed!"));
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("rproc start %d", RPROC_MISSING));
	ut_assert_nextline("Operation Failed with error (%d)", -ENODEV);
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("rproc is_running %d", RPROC_MISSING));
	ut_assert_nextline("Operation Failed with error (%d)", -ENODEV);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_rproc_missing, RPROC_TEST_FLAGS);

/* Test the usage errors */
static int cmd_test_rproc_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("rproc", 0));
	ut_assert_skip_to_line(RPROC_USAGE_LAST);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	ut_asserteq(1, run_command("rproc fish", 0));
	ut_assert_skip_to_line(RPROC_USAGE_LAST);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	/* 'rproc load' wants an address and a size as well as an ID */
	ut_asserteq(1, run_commandf("rproc load %d %lx", RPROC_DEV1,
				    RPROC_ADDR));
	ut_assert_skip_to_line(RPROC_USAGE_LAST);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	/* and a size of zero would load nothing */
	ut_asserteq(1, run_commandf("rproc load %d %lx 0", RPROC_DEV1,
				    RPROC_ADDR));
	ut_assert_nextline("\t Expect some size??");
	ut_assert_skip_to_line(RPROC_USAGE_LAST);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_rproc_usage, RPROC_TEST_FLAGS);
