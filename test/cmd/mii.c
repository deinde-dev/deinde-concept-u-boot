// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the mii command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <miiphy.h>
#include <test/cmd.h>
#include <test/ut.h>

/* bus which sandbox's emulated MDIO driver provides */
#define MII_BUS		"mdio-test"

/* the buses the command lists, with the trailing space it prints */
#define MII_DEVICES \
	"MII devices: 'mdio-test' 'mdio-ch-test@0' 'mdio-ch-test@1' "

/* last line of the help text, which ends the usage message */
#define MII_USAGE_LAST	"Addr and/or reg may be ranges, e.g. 2-7."

/**
 * probe_buses() - Make the MDIO buses available to the command
 *
 * The mii command works on the legacy MII bus list, which a driver-model MDIO
 * bus joins when it is probed. Nothing probes those buses during boot, so the
 * command finds none until something does; the mdio command probes them
 * itself, which this stands in for.
 */
static void probe_buses(void)
{
	dm_mdio_probe_devices();
}

/**
 * check_usage() - Check the usage message, which a bad command line produces
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("mii - MII utility commands");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_line(MII_USAGE_LAST);
	ut_assert_console_end();

	return 0;
}

/* Test listing the buses, and reading and writing a PHY register */
static int cmd_test_mii_base(struct unit_test_state *uts)
{
	probe_buses();

	ut_assertok(run_command("mii device", 0));
	ut_assert_nextline(MII_DEVICES);
	ut_assert_nextline("Current device: '" MII_BUS "'");
	ut_assert_console_end();

	/* a write says nothing and the value reads back */
	ut_assertok(run_command("mii write 5 0 1140", 0));
	ut_assert_console_end();

	ut_assertok(run_command("mii read 5 0", 0));
	ut_assert_nextline("1140");
	ut_assert_console_end();

	/* modify changes only the bits which the mask selects */
	ut_assertok(run_command("mii modify 5 0 0000 2040", 0));
	ut_assert_console_end();

	ut_assertok(run_command("mii read 5 0", 0));
	ut_assert_nextline("1100");
	ut_assert_console_end();

	/* the other register of the mocked PHY is separate from the first */
	ut_assertok(run_command("mii write 5 1 beef", 0));
	ut_assertok(run_command("mii read 5 1", 0));
	ut_assert_nextline("BEEF");
	ut_assert_console_end();

	/*
	 * info reads the two PHY identifier registers, which the mocked PHY
	 * does not have, so no PHY is found at any of the 32 addresses
	 */
	ut_assertok(run_command("mii info", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mii_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the ranges which an address and a register number accept */
static int cmd_test_mii_range(struct unit_test_state *uts)
{
	probe_buses();

	/* a range over the registers names each one it shows */
	ut_assertok(run_command("mii write 5 0 1234", 0));
	ut_assertok(run_command("mii read 5 0-1", 0));
	ut_assert_nextline("addr=05 reg=00 data=1234");
	ut_assert_nextline("addr=05 reg=01 data=0000");
	ut_assert_console_end();

	/* a write over a range reaches both registers */
	ut_assertok(run_command("mii write 5 0-1 4321", 0));
	ut_assertok(run_command("mii read 5 0-1", 0));
	ut_assert_nextline("addr=05 reg=00 data=4321");
	ut_assert_nextline("addr=05 reg=01 data=4321");
	ut_assert_console_end();

	/*
	 * a range over the addresses ends each address with a blank line;
	 * only address 5 answers, so the other one reports an error and the
	 * command fails
	 */
	ut_asserteq(1, run_command("mii read 5-6 0-1", 0));
	ut_assert_nextline("addr=05 reg=00 data=4321");
	ut_assert_nextline("addr=05 reg=01 data=4321");
	ut_assert_nextline_empty();
	ut_assert_nextline("Error reading from the PHY addr=06 reg=00");
	ut_assert_nextline("Error reading from the PHY addr=06 reg=01");
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mii_range, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test naming the bus to work on */
static int cmd_test_mii_device(struct unit_test_state *uts)
{
	probe_buses();

	/* naming a bus makes it current, which the listing then shows */
	ut_assertok(run_command("mii device mdio-ch-test@0", 0));
	ut_assert_console_end();

	ut_assertok(run_command("mii device", 0));
	ut_assert_nextline(MII_DEVICES);
	ut_assert_nextline("Current device: 'mdio-ch-test@0'");
	ut_assert_console_end();

	/*
	 * that bus is a channel of the mux over the first one, so it reaches
	 * the same PHY
	 */
	ut_assertok(run_command("mii write 5 0 9876", 0));
	ut_assertok(run_command("mii device " MII_BUS, 0));
	ut_assertok(run_command("mii read 5 0", 0));
	ut_assert_nextline("9876");
	ut_assert_console_end();

	/* a name which is not a bus is reported, and nothing changes */
	ut_assertok(run_command("mii device nosuch", 0));
	ut_assert_nextline("No such device: nosuch");
	ut_assert_console_end();

	ut_assertok(run_command("mii device", 0));
	ut_assert_nextline(MII_DEVICES);
	ut_assert_nextline("Current device: '" MII_BUS "'");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mii_device, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test showing a register field by field */
static int cmd_test_mii_dump(struct unit_test_state *uts)
{
	probe_buses();

	/* autonegotiation enabled, 1000 Mbps, full duplex */
	ut_assertok(run_command("mii write 5 0 1140", 0));
	ut_assertok(run_command("mii dump 5 0", 0));
	ut_assert_nextlinen("0.     (1140)");
	ut_assert_nextline("  (8000:0000) 0.15    =     0\t  reset");
	ut_assert_nextline("  (4000:0000) 0.14    =     0\t  loopback");

	/* the speed is two bits which are not next to each other */
	ut_assert_nextlinen("  (2040:0040) 0. 6,13 =   b10");
	ut_assert_nextline("  (1000:1000) 0.12    =     1\t  A/N enable");
	ut_assert_skip_to_line("  (0100:0100) 0. 8    =     1\t  duplex = full");
	ut_assert_skip_to_line("  (003f:0000) 0. 5- 0 =     0\t  (reserved)");
	ut_assert_nextline_empty();
	ut_assert_nextline_empty();
	ut_assert_console_end();

	/* only the registers clause 22 defines can be shown this way */
	ut_asserteq(1, run_command("mii dump 5 b", 0));
	ut_assert_nextlinen("The MII dump command only formats the standard");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mii_dump, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the command lines which fail or show the usage message */
static int cmd_test_mii_err(struct unit_test_state *uts)
{
	probe_buses();

	/* no PHY answers at this address */
	ut_asserteq(1, run_command("mii read 6 0", 0));
	ut_assert_nextline("Error reading from the PHY addr=06 reg=00");
	ut_assert_console_end();

	ut_asserteq(1, run_command("mii write 6 0 1234", 0));
	ut_assert_nextline("Error writing to the PHY addr=06 reg=00");
	ut_assert_console_end();

	/* a modify reads first, so it is the read which is reported */
	ut_asserteq(1, run_command("mii modify 6 0 0001 0001", 0));
	ut_assert_nextline("Error reading from the PHY addr=06 reg=00");
	ut_assert_console_end();

	/* an address is five bits, so 20 hex is one too many */
	ut_asserteq(1, run_command("mii read 20 0", 0));
	ut_assert_nextline("Incorrect PHY address. Range should be 0-31");
	ut_assertok(check_usage(uts));

	/* a sub-command is needed */
	ut_asserteq(1, run_command("mii", 0));
	ut_assertok(check_usage(uts));

	/* a word which is not a sub-command */
	ut_asserteq(1, run_command("mii bogus", 0));
	ut_assertok(check_usage(uts));

	/*
	 * device and dump both begin with a d, so one letter is not enough to
	 * tell them apart and neither is accepted
	 */
	ut_asserteq(1, run_command("mii d", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_mii_err, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);
