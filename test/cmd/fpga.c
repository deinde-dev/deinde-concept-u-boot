// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the fpga command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <env.h>
#include <test/cmd.h>
#include <test/ut.h>

/* address the tests hand to the command, which never reads it */
#define FPGA_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x8000)

/*
 * Names of the functions which log the messages the tests check,
 * right-aligned in CONFIG_LOGF_FUNC_PAD (20) characters as the log emits them
 */
#define FUNC_CHECK	"do_fpga_check_params"
#define FUNC_WRAPPER	"     do_fpga_wrapper"
#define FUNC_LOADMK	"      do_fpga_loadmk"
#define FUNC_LOADS	"       do_fpga_loads"
#define FUNC_INFO	"           fpga_info"
#define FUNC_DEV_INFO	"       fpga_dev_info"
#define FUNC_VALIDATE	"       fpga_validate"

/* the lines of the usage message which every board with the command shows */
#define USAGE_INFO \
	"fpga info   [dev]                  List known device information"
#define USAGE_DUMP \
	"fpga dump   <dev> <address> <size> Load device to memory buffer"
#define USAGE_LOAD \
	"fpga load   <dev> <address> <size> Load device from memory buffer"

/**
 * check_log() - Check the next console line, which the command logs
 *
 * The log format shows the function name only when CONFIG_LOGF_FUNC is
 * enabled, which is true of sandbox but not of the other sandbox variants,
 * so the expected line differs between them.
 *
 * @uts: Test state
 * @func: Function which emits the message
 * @msg: Rest of the line
 * Return: 0 if OK, 1 on failure
 */
static int check_log(struct unit_test_state *uts, const char *func,
		     const char *msg)
{
	if (IS_ENABLED(CONFIG_LOGF_FUNC))
		ut_assert_nextline("%s() %s", func, msg);
	else
		ut_assert_nextline("%s", msg);

	return 0;
}

/* Test 'fpga info' on a board with no FPGA device of its own */
static int cmd_test_fpga_base(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("fpga info", 0));
	ut_assertok(check_log(uts, FUNC_INFO, "No FPGA devices available."));
	ut_assert_nextline("Command 'info' failed: Error 1");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_fpga_base, UTF_CONSOLE);

/* Test how the command picks the device, including the 'fpga' variable */
static int cmd_test_fpga_dev(struct unit_test_state *uts)
{
	/* a device which is not in the table is reported by number */
	ut_asserteq(1, run_command("fpga info 2", 0));
	ut_assertok(check_log(uts, FUNC_DEV_INFO, "Invalid device number 2"));
	ut_assert_nextline("Command 'info' failed: Error 1");

	/* the 'fpga' variable is used in place of the argument */
	ut_assertok(env_set("fpga", "5"));
	ut_asserteq(1, run_command("fpga info 2", 0));
	ut_assertok(check_log(uts, FUNC_DEV_INFO, "Invalid device number 5"));
	ut_assert_nextline("Command 'info' failed: Error 1");
	ut_assertok(env_set("fpga", NULL));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_fpga_dev, UTF_CONSOLE);

/**
 * check_usage() - Check the console holds the command's usage message
 *
 * Only the lines which every board shows are checked; the rest of the message
 * depends on which sub-commands are built, so it is thrown away.
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("fpga - loadable FPGA image support");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline(USAGE_INFO);
	ut_assert_nextline(USAGE_DUMP);
	ut_assert_nextline(USAGE_LOAD);
	console_record_reset();

	return 0;
}

/* Test the arguments the command refuses */
static int cmd_test_fpga_usage(struct unit_test_state *uts)
{
	/* with no sub-command it shows the usage message */
	ut_asserteq(1, run_command("fpga", 0));
	ut_assertok(check_usage(uts));

	/* a sub-command which does not exist */
	ut_asserteq(1, run_command("fpga bogus", 0));
	ut_assertok(check_log(uts, FUNC_WRAPPER, "Non existing command"));

	/* more arguments than the sub-command takes */
	ut_asserteq(1, run_commandf("fpga load 0 %lx 10 20", (ulong)FPGA_ADDR));
	ut_assertok(check_log(uts, FUNC_WRAPPER, "Too many parameters passed"));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_fpga_usage, UTF_CONSOLE);

/* Test the argument checks shared by the sub-commands which load a bitstream */
static int cmd_test_fpga_load(struct unit_test_state *uts)
{
	/* fewer arguments than the sub-command takes */
	ut_asserteq(1, run_command("fpga load 0", 0));
	ut_assertok(check_log(uts, FUNC_CHECK,
			      "Incorrect number of parameters passed"));
	ut_assert_nextline("Command 'load' failed: Error 1");

	/* a zero address */
	ut_asserteq(1, run_command("fpga load 0 0 10", 0));
	ut_assertok(check_log(uts, FUNC_CHECK, "Zero fpga_data address"));
	ut_assert_nextline("Command 'load' failed: Error 1");

	/* a zero size */
	ut_asserteq(1, run_commandf("fpga load 0 %lx 0", (ulong)FPGA_ADDR));
	ut_assertok(check_log(uts, FUNC_CHECK, "Zero size"));
	ut_assert_nextline("Command 'load' failed: Error 1");

	/*
	 * with the arguments in order the command reaches the device table,
	 * which is empty here
	 */
	ut_asserteq(1, run_commandf("fpga load 0 %lx 10", (ulong)FPGA_ADDR));
	ut_assertok(check_log(uts, FUNC_VALIDATE, "Invalid device number 0"));
	ut_assert_nextline("Command 'load' failed: Error 1");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_fpga_load, UTF_CONSOLE);

/* Test 'fpga dump', which takes the same arguments as 'fpga load' */
static int cmd_test_fpga_dump(struct unit_test_state *uts)
{
	ut_asserteq(1, run_commandf("fpga dump 0 %lx 10", (ulong)FPGA_ADDR));
	ut_assertok(check_log(uts, FUNC_VALIDATE, "Invalid device number 0"));
	ut_assert_nextline("Command 'dump' failed: Error 1");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_fpga_dump, UTF_CONSOLE);

/* Test 'fpga loadb' and 'fpga loadp', the other forms of a full load */
static int cmd_test_fpga_loadb(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_CMD_FPGA_LOADB) ||
	    !IS_ENABLED(CONFIG_CMD_FPGA_LOADP) ||
	    !IS_ENABLED(CONFIG_CMD_FPGA_LOADBP))
		return -EAGAIN;

	ut_asserteq(1, run_commandf("fpga loadb 0 %lx 10", (ulong)FPGA_ADDR));
	ut_assertok(check_log(uts, FUNC_VALIDATE, "Invalid device number 0"));
	ut_assert_nextline("Command 'loadb' failed: Error 1");

	ut_asserteq(1, run_commandf("fpga loadp 0 %lx 10", (ulong)FPGA_ADDR));
	ut_assertok(check_log(uts, FUNC_VALIDATE, "Invalid device number 0"));
	ut_assert_nextline("Command 'loadp' failed: Error 1");

	ut_asserteq(1, run_commandf("fpga loadbp 0 %lx 10", (ulong)FPGA_ADDR));
	ut_assertok(check_log(uts, FUNC_VALIDATE, "Invalid device number 0"));
	ut_assert_nextline("Command 'loadbp' failed: Error 1");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_fpga_loadb, UTF_CONSOLE);

/* Test 'fpga loadfs', which needs every one of its seven arguments */
static int cmd_test_fpga_loadfs(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_CMD_FPGA_LOADFS))
		return -EAGAIN;

	/* the device and partition cannot be left out */
	ut_asserteq(1, run_commandf("fpga loadfs 0 %lx 10 1000 mmc file",
				    (ulong)FPGA_ADDR));
	ut_assertok(check_log(uts, FUNC_CHECK,
			      "Incorrect number of parameters passed"));
	ut_assert_nextline("Command 'loadfs' failed: Error 1");

	ut_asserteq(1, run_commandf("fpga loadfs 0 %lx 10 1000 mmc 0:1 file",
				    (ulong)FPGA_ADDR));
	ut_assertok(check_log(uts, FUNC_VALIDATE, "Invalid device number 0"));
	ut_assert_nextline("Command 'loadfs' failed: Error 1");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_fpga_loadfs, UTF_CONSOLE);

/* Test 'fpga loadmk', which takes the image address from a variable too */
static int cmd_test_fpga_loadmk(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_CMD_FPGA_LOADMK))
		return -EAGAIN;

	/* with no device named and no 'fpga' variable there is no device */
	ut_asserteq(1, run_command("fpga loadmk", 0));
	ut_assertok(check_log(uts, FUNC_LOADMK, "Invalid fpga device"));
	ut_assert_nextline("Command 'loadmk' failed: Error 1");

	/* a single argument is the device, leaving the address to come from
	 * the 'fpgadata' variable, which is not set here either
	 */
	ut_asserteq(1, run_command("fpga loadmk 0", 0));
	ut_assertok(check_log(uts, FUNC_LOADMK, "Zero fpga_data address"));
	ut_assert_nextline("Command 'loadmk' failed: Error 1");

	ut_asserteq(1, run_command("fpga loadmk 0 0", 0));
	ut_assertok(check_log(uts, FUNC_LOADMK, "Zero fpga_data address"));
	ut_assert_nextline("Command 'loadmk' failed: Error 1");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_fpga_loadmk, UTF_CONSOLE);

/* Test 'fpga loads', which refuses a bitstream that is not secure */
static int cmd_test_fpga_loads(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_CMD_FPGA_LOAD_SECURE))
		return -EAGAIN;

	/* the two flags are required */
	ut_asserteq(1, run_commandf("fpga loads 0 %lx 10", (ulong)FPGA_ADDR));
	ut_assertok(check_log(uts, FUNC_LOADS, "Too few parameters passed"));
	ut_assert_nextline("Command 'loads' failed: Error 1");

	/* neither authenticated nor encrypted is a job for 'fpga load' */
	ut_asserteq(1, run_commandf("fpga loads 0 %lx 10 2 2",
				    (ulong)FPGA_ADDR));
	ut_assertok(check_log(uts, FUNC_LOADS,
			      "Use <fpga load> for NonSecure bitstream"));
	ut_assert_nextline("Command 'loads' failed: Error 1");

	/* a user key is wanted when the encryption flag asks for one */
	ut_asserteq(1, run_commandf("fpga loads 0 %lx 10 0 1",
				    (ulong)FPGA_ADDR));
	ut_assertok(check_log(uts, FUNC_LOADS, "User key not provided"));
	ut_assert_nextline("Command 'loads' failed: Error 1");

	/* with the flags in order the command reaches the device table */
	ut_asserteq(1, run_commandf("fpga loads 0 %lx 10 0 0",
				    (ulong)FPGA_ADDR));
	ut_assertok(check_log(uts, FUNC_VALIDATE, "Invalid device number 0"));
	ut_assert_nextline("Command 'loads' failed: Error 1");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_fpga_loads, UTF_CONSOLE);
