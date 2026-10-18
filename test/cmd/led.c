// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the led command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <errno.h>
#include <test/cmd.h>
#include <test/ut.h>

/* an LED of the sandbox devicetree, wired to a GPIO the tests can read back */
#define RED	"sandbox:red"

/*
 * The usage lines which are too long to write out where they are used. These
 * go through printf(), so the duty cycle needs its percent sign doubled
 */
#define HELP_STATE \
	"led <led_label> on|off|toggle\tChange LED state"
#define HELP_BLINK \
	"led <led_label> blink <blink-freq in ms>\tBlink LED (duty cycle 50%%)"

/**
 * check_usage() - Check the usage message the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("led - manage LEDs");
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline(HELP_STATE);

		/* the blink line goes with the driver support for it */
		if (IS_ENABLED(CONFIG_LED_BLINK) ||
		    IS_ENABLED(CONFIG_LED_SW_BLINK))
			ut_assert_nextline(HELP_BLINK);
		ut_assert_nextline("led <led_label>\tGet LED state");
		ut_assert_nextline("led list\t\tshow a list of LEDs");
	}
	ut_assert_console_end();

	return 0;
}

/**
 * check_state() - Check the state the command reports for the red LED
 *
 * @uts: Test state
 * @state: State expected, as the command spells it
 * Return: 0 if OK, other value on error
 */
static int check_state(struct unit_test_state *uts, const char *state)
{
	ut_assertok(run_command("led " RED, 0));
	ut_assert_nextline("LED '" RED "': %s", state);
	ut_assert_console_end();

	return 0;
}

/* Test reading an LED state and the three ways of changing it */
static int cmd_test_led_base(struct unit_test_state *uts)
{
	/* the GPIO starts low, so the LED it drives is off */
	ut_assertok(check_state(uts, "off"));

	/* a change is silent, so only a second command shows it */
	ut_assertok(run_command("led " RED " on", 0));
	ut_assert_console_end();
	ut_assertok(check_state(uts, "on"));

	ut_assertok(run_command("led " RED " off", 0));
	ut_assert_console_end();
	ut_assertok(check_state(uts, "off"));

	/* toggle moves the LED to whichever state it is not in */
	ut_assertok(run_command("led " RED " toggle", 0));
	ut_assert_console_end();
	ut_assertok(check_state(uts, "on"));

	ut_assertok(run_command("led " RED " toggle", 0));
	ut_assert_console_end();
	ut_assertok(check_state(uts, "off"));

	return 0;
}
CMD_TEST(cmd_test_led_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test that a leading part of a state name is enough */
static int cmd_test_led_abbrev(struct unit_test_state *uts)
{
	/* 't' can only be toggle */
	ut_assertok(run_command("led " RED " t", 0));
	ut_assert_console_end();
	ut_assertok(check_state(uts, "on"));

	/*
	 * 'o' is a leading part of both off and on, and the first state to
	 * match wins, so it means off
	 */
	ut_assertok(run_command("led " RED " o", 0));
	ut_assert_console_end();
	ut_assertok(check_state(uts, "off"));

	return 0;
}
CMD_TEST(cmd_test_led_abbrev, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the list of LEDs, which shows one line for each */
static int cmd_test_led_list(struct unit_test_state *uts)
{
	/*
	 * The list does not probe anything, so every LED is inactive until
	 * something asks for one. The label is set as the device is bound,
	 * either from the devicetree or from the colour and function, so a
	 * line appears for each of them even so
	 */
	ut_assertok(run_command("led list", 0));
	ut_assert_nextline("sandbox:red     <inactive>");
	ut_assert_nextline("sandbox:green   <inactive>");
	ut_assert_nextline("sandbox:default_on <inactive>");
	ut_assert_nextline("default_off     <inactive>");
	ut_assert_nextline("red:status-20   <inactive>");
	ut_assert_nextline("green:status    <inactive>");
	ut_assert_nextline(":status         <inactive>");
	ut_assert_nextline("green:          <inactive>");
	ut_assert_nextline("sandbox:function <inactive>");
	ut_assert_console_end();

	/* asking for a state probes that LED, so the list then shows it */
	ut_assertok(run_command("led " RED " on", 0));
	ut_assertok(run_command("led list", 0));
	ut_assert_nextline("sandbox:red     on");
	ut_assert_nextline("sandbox:green   <inactive>");
	ut_assert_skip_to_line("sandbox:function <inactive>");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_led_list, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test an LED label which no device has */
static int cmd_test_led_missing(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("led sandbox:blue", 0));
	ut_assert_nextline("LED 'sandbox:blue' not found (err=%d)", -ENODEV);
	ut_assert_console_end();

	/* the label is looked up before the state is set, so this fails too */
	ut_asserteq(1, run_command("led sandbox:blue on", 0));
	ut_assert_nextline("LED 'sandbox:blue' not found (err=%d)", -ENODEV);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_led_missing, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test blinking, which the GPIO driver cannot do */
static int cmd_test_led_blink(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_LED_BLINK))
		return -EAGAIN;

	/*
	 * led_gpio has no set_period() method, so the period is refused and
	 * the LED is left as it is
	 */
	ut_asserteq(1, run_command("led " RED " blink 1000", 0));
	ut_assert_nextline("LED '" RED "' operation failed (err=%d)", -ENOSYS);
	ut_assert_console_end();
	ut_assertok(check_state(uts, "off"));

	return 0;
}
CMD_TEST(cmd_test_led_blink, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the arguments the command refuses */
static int cmd_test_led_usage(struct unit_test_state *uts)
{
	/* the command needs a label to work on */
	ut_asserteq(1, run_command("led", 0));
	ut_assertok(check_usage(uts));

	/* a state it does not know is refused before the LED is looked up */
	ut_asserteq(1, run_command("led " RED " bogus", 0));
	ut_assertok(check_usage(uts));
	ut_assertok(check_state(uts, "off"));

	/* blink needs the period as well */
	ut_asserteq(1, run_command("led " RED " blink", 0));
	ut_assertok(check_usage(uts));

	/*
	 * The command takes no options, so one is refused before it runs,
	 * rather than being looked up as a label
	 */
	ut_asserteq(1, run_command("led -x", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_led_usage, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);
