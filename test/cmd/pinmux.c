// SPDX-License-Identifier: GPL-2.0+
/*
 * Executes tests for pinmux command
 *
 * Copyright (C) 2021, STMicroelectronics - All Rights Reserved
 */

#include <command.h>
#include <test/cmd.h>
#include <test/ut.h>

/*
 * The pinctrl-single-no-width node has no register width, so that device
 * never probes and the driver logs the reason each time the command walks the
 * uclass. The line lands in the middle of the output, so step over it rather
 * than asserting on it, since its text depends on CONFIG_LOGF_FUNC.
 */
#define SINGLE_PINS \
	"| pinctrl-single-pins           | single-pinctrl                | root_driver"

/* Select a pin-controller, since the command remembers the last one */
static int select_pinctrl(struct unit_test_state *uts, const char *name)
{
	ut_assertok(run_commandf("pinmux dev %s", name));
	ut_assert_nextline("dev: %s", name);
	ut_assert_console_end();

	return 0;
}

/* Check the usage message, which every bad argument shows */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("pinmux - show pin-controller muxing");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline(
		"pinmux list                     - list UCLASS_PINCTRL devices");
	ut_assert_nextline(
		"pinmux dev [pincontroller-name] - select pin-controller device");
	ut_assert_nextlinen(
		"pinmux status [-a | pin-name]   - print pin-controller muxing");
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}

/* Test the muxing shown for the selected pin-controller */
static int cmd_test_pinmux_base(struct unit_test_state *uts)
{
	ut_assertok(select_pinctrl(uts, "pinctrl"));

	ut_assertok(run_command("pinmux status", 0));
	ut_assert_nextlinen("P0        : UART TX.");
	ut_assert_nextlinen("P1        : UART RX.");
	ut_assert_nextlinen("P2        : I2S SCK.");
	ut_assert_nextlinen("P3        : I2S SD.");
	ut_assert_nextlinen("P4        : I2S WS.");
	ut_assert_nextlinen("P5        : GPIO0 bias-pull-up input-disable.");
	ut_assert_nextlinen("P6        : GPIO1 drive-open-drain.");
	ut_assert_nextlinen("P7        : GPIO2 bias-pull-down input-enable.");
	ut_assert_nextlinen("P8        : GPIO3 bias-disable.");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_pinmux_base, UTF_CONSOLE);

/* Test selecting a pin-controller and showing which one is selected */
static int cmd_test_pinmux_dev(struct unit_test_state *uts)
{
	ut_assertok(select_pinctrl(uts, "pinctrl-gpio"));

	/* with no name it shows the one already selected */
	ut_assertok(run_command("pinmux dev", 0));
	ut_assert_nextline("dev: pinctrl-gpio");
	ut_assert_console_end();

	/*
	 * A name which no pin-controller has fails, and leaves nothing
	 * selected, which is how the command starts out
	 */
	ut_asserteq(1, run_command("pinmux dev fish", 0));
	ut_assert_nextline("Can't get the pin-controller: fish!");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_pinmux_dev, UTF_CONSOLE);

/* Test the listing of the pin-controllers sandbox provides */
static int cmd_test_pinmux_list(struct unit_test_state *uts)
{
	ut_assertok(run_command("pinmux list", 0));
	ut_assert_nextline(
		"| Device                        | Driver                        | Parent");
	ut_assert_nextline(
		"| pinctrl-gpio                  | sandbox_pinctrl_gpio          | root_driver");
	ut_assert_nextline(
		"| gpios@c000                    | qcom_pmic_pinctrl             | gpios@c000");
	ut_assert_nextline(
		"| pins-test                     | sandbox_pins_test             | root_driver");
	ut_assert_nextline(
		"| pinctrl                       | sandbox_pinctrl               | root_driver");
	ut_assert_skip_to_line(SINGLE_PINS);
	ut_assert_nextline(
		"| pinctrl-single-bits           | single-pinctrl                | root_driver");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_pinmux_list, UTF_CONSOLE);

/* Test showing the muxing of every pin-controller */
static int cmd_test_pinmux_status_all(struct unit_test_state *uts)
{
	ut_assertok(run_command("pinmux status -a", 0));
	ut_assert_nextline("--------------------------");
	ut_assert_nextline("pinctrl-gpio:");
	ut_assert_nextlinen("a0        : gpio input .");

	/* each controller has a heading of its own */
	ut_assert_skip_to_line("pinctrl:");
	ut_assert_nextlinen("P0        : UART TX.");
	ut_assert_nextlinen("P1        : UART RX.");
	ut_assert_nextlinen("P2        : I2S SCK.");
	ut_assert_nextlinen("P3        : I2S SD.");
	ut_assert_nextlinen("P4        : I2S WS.");
	ut_assert_nextlinen("P5        : GPIO0 bias-pull-up input-disable.");
	ut_assert_nextlinen("P6        : GPIO1 drive-open-drain.");
	ut_assert_nextlinen("P7        : GPIO2 bias-pull-down input-enable.");
	ut_assert_nextlinen("P8        : GPIO3 bias-disable.");

	/* the two single-pinctrl controllers which follow have 160 pins each */
	ut_assert_skip_to_line("pinctrl-single-bits:");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_pinmux_status_all, UTF_CONSOLE);

/* Test showing the muxing of a single pin, named on the command line */
static int cmd_test_pinmux_status_pin(struct unit_test_state *uts)
{
	/* the pin may be on any controller, not just the selected one */
	ut_assertok(run_command("pinmux status a5", 0));
	ut_assert_nextlinen("a5        : gpio output .");
	ut_assert_console_end();

	ut_assertok(run_command("pinmux status P7", 0));
	ut_assert_nextlinen("P7        : GPIO2 bias-pull-down input-enable.");
	ut_assert_console_end();

	/* no controller has a pin of this name */
	ut_asserteq(1, run_command("pinmux status P9", 0));
	ut_assert_skip_to_line("P9 not found");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_pinmux_status_pin, UTF_CONSOLE);

/* Test that a repeat runs the sub-command again */
static int cmd_test_pinmux_repeat(struct unit_test_state *uts)
{
	char *const args[] = { "pinmux", "list", NULL };
	int repeatable;

	repeatable = -1;
	ut_assertok(cmd_process(0, 2, args, &repeatable, NULL));
	ut_asserteq(1, repeatable);
	console_record_reset();

	/* pressing Enter after it lists the controllers again */
	ut_assertok(cmd_process(CMD_FLAG_REPEAT, 2, args, &repeatable, NULL));
	ut_assert_nextlinen("| Device ");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_pinmux_repeat, UTF_CONSOLE);

/* Test the usage message */
static int cmd_test_pinmux_usage(struct unit_test_state *uts)
{
	/* the command needs a sub-command */
	ut_asserteq(1, run_command("pinmux", 0));
	ut_assertok(check_usage(uts));

	/* an unknown sub-command shows the usage message too */
	ut_asserteq(1, run_command("pinmux fish", 0));
	ut_assertok(check_usage(uts));

	/* 'list' takes no arguments */
	ut_asserteq(1, run_command("pinmux list extra", 0));
	ut_assertok(check_usage(uts));

	/* 'dev' takes only the name of the pin-controller */
	ut_asserteq(1, run_command("pinmux dev pinctrl extra", 0));
	ut_assertok(check_usage(uts));

	/* 'status' takes only the option or the pin name */
	ut_asserteq(1, run_command("pinmux status -a extra", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_pinmux_usage, UTF_CONSOLE);
