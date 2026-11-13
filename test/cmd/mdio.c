// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the mdio command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <test/cmd.h>
#include <test/ut.h>

/* bus which sandbox's emulated MDIO driver provides */
#define MDIO_BUS	"mdio-test"

/* last line of the help text, which ends the usage message */
#define MDIO_USAGE_LAST	"<addr> <devad>, and <reg> may be ranges"

/**
 * check_usage() - Check the usage message, which a bad command line produces
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("mdio - MDIO utility commands");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_linen(MDIO_USAGE_LAST);

	/* the help text ends with a newline, so the message does too */
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}

/* Test listing the buses, and reading and writing a PHY register */
static int cmd_test_mdio_base(struct unit_test_state *uts)
{
	/*
	 * the command probes every MDIO bus itself, so the buses appear
	 * without anything being set up first. No PHY driver is attached to
	 * any of them, so each bus is listed on its own
	 */
	ut_assertok(run_command("mdio list", 0));
	ut_assert_nextline(MDIO_BUS ":");
	ut_assert_nextline("mdio-ch-test@0:");
	ut_assert_nextline("mdio-ch-test@1:");
	ut_assert_console_end();

	/* a write says nothing and the value reads back */
	ut_assertok(run_command("mdio write " MDIO_BUS " 5 0 1140", 0));
	ut_assert_console_end();

	ut_assertok(run_command("mdio read " MDIO_BUS " 5 0", 0));
	ut_assert_nextline("Reading from bus " MDIO_BUS);
	ut_assert_nextline("PHY at address 5:");
	ut_assert_nextline("0 - 0x1140");
	ut_assert_console_end();

	/* the other register of the mocked PHY is separate from the first */
	ut_assertok(run_command("mdio write " MDIO_BUS " 5 1 abcd", 0));
	ut_assertok(run_command("mdio read " MDIO_BUS " 5 0-1", 0));
	ut_assert_nextline("Reading from bus " MDIO_BUS);
	ut_assert_nextline("PHY at address 5:");
	ut_assert_nextline("0 - 0x1140");
	ut_assert_nextline("1 - 0xabcd");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mdio_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the ways of naming the PHY to work on */
static int cmd_test_mdio_phydev(struct unit_test_state *uts)
{
	ut_assertok(run_command("mdio write " MDIO_BUS " 5 0 4321", 0));
	ut_assert_console_end();

	/*
	 * an address on its own uses the bus of the last command, but is
	 * looked for among the Ethernet devices first
	 */
	ut_assertok(run_command("mdio read 5 0", 0));
	ut_assert_nextline("5 is not a known ethernet");
	ut_assert_nextline("Reading from bus " MDIO_BUS);
	ut_assert_nextline("PHY at address 5:");
	ut_assert_nextline("0 - 0x4321");
	ut_assert_console_end();

	/* a channel of the mux reaches the same PHY as its parent bus */
	ut_assertok(run_command("mdio read mdio-ch-test@0 5 0", 0));
	ut_assert_nextline("Reading from bus mdio-ch-test@0");
	ut_assert_nextline("PHY at address 5:");
	ut_assert_nextline("0 - 0x4321");
	ut_assert_console_end();

	/* no Ethernet device of this name has a PHY on an MDIO bus */
	ut_asserteq(1, run_command("mdio read eth0 0", 0));
	ut_assert_nextline("eth0 is not a known ethernet");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mdio_phydev, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the command lines which fail or show the usage message */
static int cmd_test_mdio_err(struct unit_test_state *uts)
{
	/*
	 * the bus answers only for address 5, and reports the error, but the
	 * command tells the caller nothing about it
	 */
	ut_assertok(run_command("mdio read " MDIO_BUS " 7 0", 0));
	ut_assert_nextline("Reading from bus " MDIO_BUS);
	ut_assert_nextline("PHY at address 7:");
	ut_assert_nextline("Error");
	ut_assert_console_end();

	/* the same for a device address, which a clause 22 PHY has not got */
	ut_assertok(run_command("mdio read " MDIO_BUS " 5 1.0", 0));
	ut_assert_nextline("Reading from bus " MDIO_BUS);
	ut_assert_nextline("PHY at address 5:");
	ut_assert_nextline("Error");
	ut_assert_console_end();

	/* a bus which does not exist fails, saying nothing at all */
	ut_asserteq(1, run_command("mdio read nosuch 5 0", 0));
	ut_assert_console_end();

	/* the extended registers need a PHY with a driver behind it */
	ut_asserteq(1, run_command("mdio rx eth0 0", 0));
	ut_assert_nextline("eth0 is not a known ethernet");
	ut_assert_console_end();

	/* a sub-command is needed */
	ut_asserteq(1, run_command("mdio", 0));
	ut_assertok(check_usage(uts));

	/* an option is refused before the handler runs */
	ut_asserteq(1, run_command("mdio -x", 0));
	ut_assertok(check_usage(uts));

	/* but a word which is not one of them does nothing, and succeeds */
	ut_assertok(run_command("mdio bogus", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mdio_err, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);
