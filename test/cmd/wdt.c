// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the wdt command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <errno.h>
#include <asm/state.h>
#include <test/cmd.h>
#include <test/ut.h>

/* the sandbox watchdog, whose effects the sandbox state shows */
#define SANDBOX_WDT	"wdt@0"

/* a GPIO-driven watchdog of the sandbox devicetree, marked always-running */
#define GPIO_WDT	"wdt-gpio-toggle"

/* the usage lines which are too long to write out where they are used */
#define HELP_DEV	"wdt dev [<name>] - get/set current watchdog device"
#define HELP_START	"wdt start <timeout ms> [flags] - start watchdog timer"
#define HELP_EXPIRE	"wdt expire [flags] - expire watchdog timer immediately"

/**
 * check_usage() - Check the usage message the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("wdt - Watchdog sub-system");
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("wdt list - list watchdog devices");
		ut_assert_nextline(HELP_DEV);
		ut_assert_nextline(HELP_START);
		ut_assert_nextline("wdt stop - stop watchdog timer");
		ut_assert_nextline("wdt reset - reset watchdog timer");
		ut_assert_nextline(HELP_EXPIRE);
		ut_assert_nextline_empty();
	}
	ut_assert_console_end();

	return 0;
}

/**
 * select_wdt() - Make a watchdog device the current one
 *
 * The command keeps the current device in a static, which outlives a test, so
 * each test selects the device it wants rather than relying on what an earlier
 * one left behind
 *
 * @uts: Test state
 * @name: Name of the device to select
 * Return: 0 if OK, other value on error
 */
static int select_wdt(struct unit_test_state *uts, const char *name)
{
	ut_assertok(run_commandf("wdt dev %s", name));
	ut_assert_console_end();

	return 0;
}

/* Test starting, servicing and stopping a watchdog */
static int cmd_test_wdt_base(struct unit_test_state *uts)
{
	struct sandbox_state *state = state_get_current();
	struct sandbox_wdt_info old = state->wdt;
	uint reset_count;

	ut_assertok(select_wdt(uts, SANDBOX_WDT));

	/* with no name the command shows the device it is working on */
	ut_assertok(run_command("wdt dev", 0));
	ut_assert_nextline("dev: " SANDBOX_WDT);
	ut_assert_console_end();

	/*
	 * Servicing the watchdog is silent, so only the sandbox state shows
	 * it. Check it while the watchdog is stopped: once it is running
	 * U-Boot services it by itself and the count moves on its own
	 */
	reset_count = state->wdt.reset_count;
	ut_assertok(run_command("wdt reset", 0));
	ut_assert_console_end();
	ut_asserteq(reset_count + 1, state->wdt.reset_count);

	/*
	 * The uclass reports the timeout it has set and how often U-Boot
	 * services the device, a quarter of the hw_margin_ms property of the
	 * devicetree node, which is 200ms here
	 */
	ut_assertok(run_command("wdt start 3000", 0));
	if (IS_ENABLED(CONFIG_WATCHDOG))
		ut_assert_nextline("WDT:   Started " SANDBOX_WDT
				   " with servicing every 50ms (3s timeout)");
	else
		ut_assert_nextline("WDT:   Started " SANDBOX_WDT
				   " without servicing  (3s timeout)");
	ut_assert_console_end();
	ut_asserteq(3000, state->wdt.counter);
	ut_asserteq(true, state->wdt.running);

	ut_assertok(run_command("wdt stop", 0));
	ut_assert_console_end();
	ut_asserteq(false, state->wdt.running);

	/*
	 * dm_test_wdt_base wants the sandbox watchdog as it is at start-up and
	 * a plain 'ut' runs the cmd suite before the dm one in the same
	 * U-Boot, so put the state back as it was found
	 */
	state->wdt = old;

	return 0;
}
CMD_TEST(cmd_test_wdt_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test that a sub-command may be abbreviated so long as it is unambiguous */
static int cmd_test_wdt_abbrev(struct unit_test_state *uts)
{
	ut_assertok(run_command("wdt l", 0));
	ut_assert_nextline("wdt-gpio-toggle (wdt_gpio)");
	ut_assert_nextline("wdt-gpio-level (wdt_gpio)");
	ut_assert_nextline("wdt@0 (wdt_sandbox)");
	ut_assert_console_end();

	/* 's' is a leading part of both start and stop, so it names neither */
	ut_asserteq(1, run_command("wdt s", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_wdt_abbrev, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test a watchdog which the driver cannot stop */
static int cmd_test_wdt_gpio(struct unit_test_state *uts)
{
	ut_assertok(select_wdt(uts, GPIO_WDT));

	/*
	 * The devicetree marks this one always-running, so the driver has
	 * nothing to do to start it, and its hw_margin_ms is 100ms
	 */
	ut_assertok(run_command("wdt start 3000", 0));
	if (IS_ENABLED(CONFIG_WATCHDOG))
		ut_assert_nextline("WDT:   Started " GPIO_WDT
				   " with servicing every 25ms (3s timeout)");
	else
		ut_assert_nextline("WDT:   Started " GPIO_WDT
				   " without servicing  (3s timeout)");
	ut_assert_console_end();

	/* an always-running watchdog refuses to be stopped */
	ut_asserteq(1, run_command("wdt stop", 0));
	ut_assert_nextline("Stopping watchdog timer failed (%d)", -EOPNOTSUPP);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_wdt_gpio, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the list of watchdog devices, which shows one line for each */
static int cmd_test_wdt_list(struct unit_test_state *uts)
{
	/*
	 * The list probes nothing, so it covers every device bound from the
	 * devicetree, each with the name of its driver in brackets
	 */
	ut_assertok(run_command("wdt list", 0));
	ut_assert_nextline("wdt-gpio-toggle (wdt_gpio)");
	ut_assert_nextline("wdt-gpio-level (wdt_gpio)");
	ut_assert_nextline("wdt@0 (wdt_sandbox)");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_wdt_list, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test a device name which no watchdog has */
static int cmd_test_wdt_missing(struct unit_test_state *uts)
{
	ut_assertok(select_wdt(uts, SANDBOX_WDT));

	ut_asserteq(1, run_command("wdt dev nosuch", 0));
	ut_assert_nextline("Can't get the watchdog timer: nosuch");
	ut_assert_console_end();

	/* the device chosen before the failed lookup is still the current one */
	ut_assertok(run_command("wdt dev", 0));
	ut_assert_nextline("dev: " SANDBOX_WDT);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_wdt_missing, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the arguments the command refuses */
static int cmd_test_wdt_usage(struct unit_test_state *uts)
{
	/* the command needs a sub-command */
	ut_asserteq(1, run_command("wdt", 0));
	ut_assertok(check_usage(uts));

	/* as it does for a sub-command it does not know */
	ut_asserteq(1, run_command("wdt bogus", 0));
	ut_assertok(check_usage(uts));

	/* list takes nothing at all */
	ut_asserteq(1, run_command("wdt list extra", 0));
	ut_assertok(check_usage(uts));

	/* start needs the timeout, which it checks before the device */
	ut_assertok(select_wdt(uts, SANDBOX_WDT));
	ut_asserteq(1, run_command("wdt start", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_wdt_usage, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);
