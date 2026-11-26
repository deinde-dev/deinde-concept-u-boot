// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the dm command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <dm/uclass-id.h>
#include <test/cmd.h>
#include <test/ut.h>

/* The first two lines of 'dm tree' output */
#define TREE_HEADER	" Class     Seq    Probed  Driver                Name"
#define TREE_RULE \
	"-----------------------------------------------------------"

/* The root device, which every board has and which is always active */
#define TREE_ROOT \
	" root          0  [ + ]   root_driver           root_driver"

/* The cros-ec device of sandbox and the two children it has */
#define TREE_CROS_EC \
" cros-ec       0  [ + ]   google_cros_ec_sandb  cros-ec"
#define TREE_PWM_FIRST \
" pwm           0  [   ]   cros_ec_pwm           |-- cros-ec-pwm"
#define TREE_PWM_LAST \
" pwm           0  [   ]   cros_ec_pwm           `-- cros-ec-pwm"
#define TREE_KEYB_FIRST \
" keyboard      0  [ + ]   google_cros_ec_keyb   |-- keyboard-controller"
#define TREE_KEYB_LAST \
" keyboard      0  [ + ]   google_cros_ec_keyb   `-- keyboard-controller"

/**
 * check_cros_ec_tree() - Check the 'dm tree' output for the cros-ec device
 *
 * @uts: Test state
 * @sorted: true if -s was given, so that the children come out in order of
 *	uclass ID rather than the order of the devicetree
 * Return: 0 if OK, 1 on failure
 */
static int check_cros_ec_tree(struct unit_test_state *uts, bool sorted)
{
	ut_assert_nextline(TREE_HEADER);
	ut_assert_nextline(TREE_RULE);
	ut_assert_nextline(TREE_CROS_EC);
	if (sorted) {
		ut_assert_nextline(TREE_KEYB_FIRST);
		ut_assert_nextline(TREE_PWM_LAST);
	} else {
		ut_assert_nextline(TREE_PWM_FIRST);
		ut_assert_nextline(TREE_KEYB_LAST);
	}
	ut_assert_console_end();

	return 0;
}

/* Test 'dm tree', which shows every device */
static int cmd_test_dm_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("dm tree", 0));
	ut_assert_nextline(TREE_HEADER);
	ut_assert_nextline(TREE_RULE);
	ut_assert_nextline(TREE_ROOT);

	/* the rest is every device on the board, which is not checked here */
	console_record_reset();

	/* the name 'root' asks for the whole tree as well */
	ut_assertok(run_command("dm tree root", 0));
	ut_assert_nextline(TREE_HEADER);
	ut_assert_nextline(TREE_RULE);
	ut_assert_nextline(TREE_ROOT);
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_dm_base, UTF_CONSOLE);

/* Test how 'dm tree' matches the device name it is given */
static int cmd_test_dm_tree_name(struct unit_test_state *uts)
{
	/* a name which matches nothing leaves just the header */
	ut_assertok(run_command("dm tree nosuchdevice", 0));
	ut_assert_nextline(TREE_HEADER);
	ut_assert_nextline(TREE_RULE);
	ut_assert_console_end();

	ut_assertok(run_command("dm tree -e nosuchdevice", 0));
	ut_assert_nextline(TREE_HEADER);
	ut_assert_nextline(TREE_RULE);
	ut_assert_console_end();

	/* the devices below are sandbox's own */
	if (!IS_ENABLED(CONFIG_SANDBOX))
		return 0;

	/* a name on its own must match a device exactly */
	ut_assertok(run_command("dm tree cros-ec", 0));
	ut_assertok(check_cros_ec_tree(uts, false));

	/* with -e the name is a prefix, so a shorter one finds the same device */
	ut_assertok(run_command("dm tree -e cros-e", 0));
	ut_assertok(check_cros_ec_tree(uts, false));

	/* without -e that same prefix matches nothing */
	ut_assertok(run_command("dm tree cros-e", 0));
	ut_assert_nextline(TREE_HEADER);
	ut_assert_nextline(TREE_RULE);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_dm_tree_name, UTF_CONSOLE);

/* Test that 'dm tree -s' sorts the children of a device by uclass ID */
static int cmd_test_dm_tree_sort(struct unit_test_state *uts)
{
	/* the devices below are sandbox's own */
	if (!IS_ENABLED(CONFIG_SANDBOX))
		return 0;

	/*
	 * cros-ec has a pwm and a keyboard child, in that order in the
	 * devicetree, and UCLASS_KEYBOARD comes before UCLASS_PWM
	 */
	ut_assertok(run_command("dm tree -s cros-ec", 0));
	ut_assertok(check_cros_ec_tree(uts, true));

	/* the two options work together, in either order */
	ut_assertok(run_command("dm tree -e -s cros-e", 0));
	ut_assertok(check_cros_ec_tree(uts, true));

	ut_assertok(run_command("dm tree -s -e cros-e", 0));
	ut_assertok(check_cros_ec_tree(uts, true));

	return 0;
}
CMD_TEST(cmd_test_dm_tree_sort, UTF_CONSOLE);

/* Test 'dm uclass', which lists the devices in each uclass */
static int cmd_test_dm_uclass(struct unit_test_state *uts)
{
	/* the root uclass comes first, with the root device in it */
	ut_assertok(run_command("dm uclass", 0));
	ut_assert_nextline("uclass %d: root", UCLASS_ROOT);
	ut_assert_nextlinen("0   * root_driver @ ");
	ut_assert_nextline_empty();

	/* the rest is every uclass on the board, which is not checked here */
	console_record_reset();

	/* the name 'root' asks for all of them as well */
	ut_assertok(run_command("dm uclass root", 0));
	ut_assert_nextline("uclass %d: root", UCLASS_ROOT);
	ut_assert_nextlinen("0   * root_driver @ ");
	ut_assert_nextline_empty();
	console_record_reset();

	/* a name which matches no uclass shows nothing at all */
	ut_assertok(run_command("dm uclass nosuchclass", 0));
	ut_assert_console_end();

	ut_assertok(run_command("dm uclass -e nosuchclass", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_dm_uclass, UTF_CONSOLE);

/* Test how 'dm uclass' matches the uclass name it is given */
static int cmd_test_dm_uclass_name(struct unit_test_state *uts)
{
	/* the w1 uclasses are empty on sandbox, so the output is short */
	if (!IS_ENABLED(CONFIG_W1) || !IS_ENABLED(CONFIG_W1_EEPROM))
		return -EAGAIN;

	/* a name on its own must match a uclass exactly */
	ut_assertok(run_command("dm uclass w1", 0));
	ut_assert_nextline("uclass %d: w1", UCLASS_W1);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	/* with -e the name is a prefix, so w1_eeprom is shown as well */
	ut_assertok(run_command("dm uclass -e w1", 0));
	ut_assert_nextline("uclass %d: w1", UCLASS_W1);
	ut_assert_nextline_empty();
	ut_assert_nextline("uclass %d: w1_eeprom", UCLASS_W1_EEPROM);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_dm_uclass_name, UTF_CONSOLE);

/* Test the sub-commands which list drivers */
static int cmd_test_dm_drivers(struct unit_test_state *uts)
{
	ut_assertok(run_command("dm compat", 0));
	ut_assert_nextline("Driver                Compatible");
	ut_assert_nextline("--------------------------------");

	/* the rest is every driver in the image, which is not checked here */
	console_record_reset();

	ut_assertok(run_command("dm drivers", 0));
	ut_assert_nextline(
		"Driver                    uid uclass               Devices");
	ut_assert_nextline(
		"----------------------------------------------------------");
	console_record_reset();

	ut_assertok(run_command("dm static", 0));
	ut_assert_nextline("Driver                    Address");
	ut_assert_nextline("---------------------------------");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_dm_drivers, UTF_CONSOLE);

/* Test 'dm devres', which shows the resources attached to each device */
static int cmd_test_dm_devres(struct unit_test_state *uts)
{
	/* without this option the sub-command shows nothing */
	if (!IS_ENABLED(CONFIG_DEBUG_DEVRES))
		return -EAGAIN;

	ut_assertok(run_command("dm devres", 0));
	ut_assert_nextline("- root_driver");

	/* the rest is every device on the board, which is not checked here */
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_dm_devres, UTF_CONSOLE);

/* Test 'dm mem', which summarises the memory driver model uses */
static int cmd_test_dm_mem(struct unit_test_state *uts)
{
	/* the sub-command exists only with this option */
	if (!IS_ENABLED(CONFIG_DM_STATS))
		return -EAGAIN;

	ut_assertok(run_command("dm mem", 0));

	/* the sizes and counts are in hex and depend on the build */
	ut_assert_nextlinen("Struct sizes: udevice ");
	ut_assert_nextlinen("Memory: device ");
	ut_assert_nextline_empty();
	ut_assert_nextline("Attached type    Count   Size    Cur   Tags   Save");
	ut_assert_nextline("---------------  -----  -----  -----  -----  -----");
	ut_assert_nextlinen("plat   ");

	/* the rest is a line per attached type, which is not checked here */
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_dm_mem, UTF_CONSOLE);

/**
 * expect_usage() - Run a command which shows the usage message
 *
 * @uts: Test state
 * @cmd: Command to run
 * Return: 0 if OK, 1 on failure
 */
static int expect_usage(struct unit_test_state *uts, const char *cmd)
{
	ut_asserteq(1, run_command(cmd, 0));
	ut_assert_nextline("dm - Driver model low level access");

	/* the rest is the usage text, which this test does not check */
	console_record_reset();

	return 0;
}

/* Test what the command does with an option it does not know */
static int cmd_test_dm_errors(struct unit_test_state *uts)
{
	/* 'tree' names the parameter it cannot make sense of */
	ut_assertok(run_command("dm tree -x", 0));
	ut_assert_nextline("Unknown parameter: -x");
	ut_assert_console_end();

	/* 'uclass' takes it as the name of a uclass, which matches nothing */
	ut_assertok(run_command("dm uclass -x", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_dm_errors, UTF_CONSOLE);

/* Test the arguments which show the usage message */
static int cmd_test_dm_usage(struct unit_test_state *uts)
{
	/* a sub-command is required, and must be one of those on offer */
	ut_assertok(expect_usage(uts, "dm"));
	ut_assertok(expect_usage(uts, "dm other"));

	/* these sub-commands take no arguments */
	ut_assertok(expect_usage(uts, "dm compat extra"));
	ut_assertok(expect_usage(uts, "dm devres extra"));
	ut_assertok(expect_usage(uts, "dm drivers extra"));
	ut_assertok(expect_usage(uts, "dm static extra"));
	if (IS_ENABLED(CONFIG_DM_STATS))
		ut_assertok(expect_usage(uts, "dm mem extra"));

	/* 'tree' takes two options and a name, 'uclass' one option and a name */
	ut_assertok(expect_usage(uts, "dm tree -s -e name extra"));
	ut_assertok(expect_usage(uts, "dm uclass -e name extra"));

	return 0;
}
CMD_TEST(cmd_test_dm_usage, UTF_CONSOLE);
