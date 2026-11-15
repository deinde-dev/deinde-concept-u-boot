// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the net command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <dm.h>
#include <net.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

/* Ethernet device the tests work on */
#define NET_DEV		"eth@10002000"

/* Last line of the help text, which ends the usage message */
#define USAGE_LAST \
	"net stats <device> - dump statistics for specified device"

/* List the Ethernet devices */
static int cmd_test_net_base(struct unit_test_state *uts)
{
	struct udevice *dev;

	/*
	 * An earlier test may have left another device current, so choose the
	 * one the listing is checked against. The listing itself probes
	 * nothing; this probes the device it is given.
	 */
	ut_assertok(uclass_get_device_by_name(UCLASS_ETH, NET_DEV, &dev));
	eth_set_dev(dev);

	/*
	 * Devices come out in the order they were bound, not that of their
	 * sequence numbers, and every line but the current device's ends in
	 * the space before the empty 'active' field.
	 *
	 * Only ethaddr and eth6addr are in the default environment, so the
	 * other six addresses are gone once a test has reset it; check the
	 * device and its sequence number for those.
	 */
	ut_assertok(run_command("net list", 0));
	ut_assert_nextline("eth0 : eth@10002000 02:00:11:22:33:44 active");
	ut_assert_nextlinen("eth5 : eth@10003000 ");
	ut_assert_nextlinen("eth3 : sbe5 ");
	ut_assert_nextline("eth6 : eth@10004000 02:00:11:22:33:47 ");
	ut_assert_nextlinen("eth8 : phy-test-eth ");
	ut_assert_nextlinen("eth4 : dsa-test-eth ");
	ut_assert_nextlinen("eth2 : lan0 ");
	ut_assert_nextlinen("eth7 : lan1 ");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_net_base, UTF_CONSOLE);

/* Dump the counters of a device whose driver keeps none */
static int cmd_test_net_stats(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("net stats " NET_DEV, 0));
	ut_assert_nextline("Driver does not implement stats dump!");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_net_stats, UTF_CONSOLE);

/* Dump the counters of a device which does not exist */
static int cmd_test_net_stats_missing(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("net stats nodev", 0));
	ut_assert_nextline("Could not find device nodev");

	/* the sequence-number alias is not a device name */
	ut_asserteq(1, run_command("net stats eth0", 0));
	ut_assert_nextline("Could not find device eth0");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_net_stats_missing, UTF_CONSOLE);

/* Check that neither sub-command is reported as repeatable */
static int cmd_test_net_repeat(struct unit_test_state *uts)
{
	char *const list_args[] = { "net", "list", NULL };
	char *const stats_args[] = { "net", "stats", NET_DEV, NULL };
	int repeatable;

	/*
	 * The net command itself is marked repeatable, so without the
	 * dispatcher narrowing it, both sub-commands would look repeatable
	 */
	repeatable = 1;
	ut_assertok(cmd_process(0, 2, list_args, &repeatable, NULL));
	ut_asserteq(0, repeatable);
	console_record_reset();

	repeatable = 1;
	ut_asserteq(1, cmd_process(0, 3, stats_args, &repeatable, NULL));
	ut_asserteq(0, repeatable);
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_net_repeat, UTF_CONSOLE);

/* Check the arguments the command refuses */
static int cmd_test_net_usage(struct unit_test_state *uts)
{
	/* a sub-command is needed */
	ut_asserteq(1, run_command("net", 0));
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_nextline_empty();

	/* and one the command has */
	ut_asserteq(1, run_command("net bogus", 0));
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_nextline_empty();

	/* 'list' takes nothing */
	ut_asserteq(1, run_command("net list extra", 0));
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_nextline_empty();

	/* 'stats' takes a device, and only one */
	ut_asserteq(1, run_command("net stats", 0));
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_nextline_empty();

	ut_asserteq(1, run_command("net stats " NET_DEV " extra", 0));
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_nextline_empty();

	/* the command has no options */
	ut_asserteq(1, run_command("net -x", 0));
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_net_usage, UTF_CONSOLE);
