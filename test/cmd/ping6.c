// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the ping6 command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <dm.h>
#include <env.h>
#include <net.h>
#include <net6.h>
#include <time.h>
#include <asm/eth.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

/* this needs the IPv6 types which net6.h brings in, so it comes last */
#include <ndisc.h>

/* Ethernet device the tests use, and its sequence number */
#define PING6_ETHACT	"eth@10002000"
#define PING6_SEQ	0

/* Address, prefix length and router the tests give the board */
#define PING6_ADDR	"2001:db8::1/64"
#define PING6_ROUTER	"2001:db8::ff"

/* Host the tests ping, which the sandbox device answers, and its full form */
#define PING6_HOST	"2001:db8::2"
#define PING6_FULL	"2001:0db8:0000:0000:0000:0000:0000:0002"

/* Last line of the help text, which ends the usage message */
#define USAGE_LAST	"ping6 pingAddress"

/* Time to move the sandbox clock on by for each idle poll, in ms */
#define PING6_JUMP	6000

/**
 * sb_ping6_handler() - move the sandbox clock on for each idle poll
 *
 * With its responses turned off the sandbox device answers no neighbour
 * solicitation, so the command sends three of them five seconds apart before
 * giving up, which is fifteen seconds of real time. The clock is moved past
 * the timeout as U-Boot looks for a reply, rather than as it sends, since
 * ndisc_request() starts the timer only after the packet has gone out; that
 * brings the test down to a few milliseconds.
 *
 * @dev: sandbox Ethernet device
 * Return: -EAGAIN, since no packet is ever queued
 */
static int sb_ping6_handler(struct udevice *dev)
{
	timer_test_add_offset(PING6_JUMP);

	return -EAGAIN;
}

/**
 * ping6_setup() - give the board an address and a router to ping from
 *
 * The router is named so that the network loop does not go looking for one
 * with a router solicitation. That search happens on the first IPv6 command
 * alone, so a test which let it run would behave differently on its own and
 * in a suite.
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int ping6_setup(struct unit_test_state *uts)
{
	ut_assertok(env_set("ethact", PING6_ETHACT));
	ut_assertok(run_command("setenv netretry no", 0));
	ut_assertok(run_command("setenv ip6addr " PING6_ADDR, 0));
	ut_assertok(run_command("setenv gatewayip6 " PING6_ROUTER, 0));

	return 0;
}

/**
 * ping6_restore() - put back the sandbox device and the network state
 *
 * The setenv command is used for the addresses, since on_ip6addr() and
 * on_gatewayip6() ignore a programmatic write. Neighbour discovery is reset
 * because the command leaves its solicitation outstanding when it gives up,
 * which a later test using the network would then wait for.
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int ping6_restore(struct unit_test_state *uts)
{
	sandbox_eth_disable_response(PING6_SEQ, false);
	sandbox_eth_set_rx_handler(PING6_SEQ, NULL);
	ndisc_init();
	ut_assertok(env_set("ethact", NULL));
	ut_assertok(run_command("setenv ip6addr", 0));
	ut_assertok(run_command("setenv gatewayip6", 0));
	ut_assertok(run_command("setenv netretry", 0));

	return 0;
}

/* Ping a host which answers */
static int cmd_test_ping6_base(struct unit_test_state *uts)
{
	ut_assertok(ping6_setup(uts));

	/* the sandbox device answers the solicitation and then the ping */
	ut_assertok(run_command("ping6 " PING6_HOST, 0));
	ut_assert_nextline("Using %s device", PING6_ETHACT);
	ut_assert_nextline("host %s is alive", PING6_FULL);
	ut_assert_console_end();

	/* the host's MAC address is known now, so it is pinged directly */
	ut_assertok(run_command("ping6 " PING6_HOST, 0));
	ut_assert_nextline("Using %s device", PING6_ETHACT);
	ut_assert_nextline("host %s is alive", PING6_FULL);
	ut_assert_console_end();

	ut_assertok(ping6_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_ping6_base, UTF_CONSOLE);

/* Ping a host which does not answer */
static int cmd_test_ping6_fail(struct unit_test_state *uts)
{
	ut_assertok(ping6_setup(uts));
	sandbox_eth_disable_response(PING6_SEQ, true);
	sandbox_eth_set_rx_handler(PING6_SEQ, sb_ping6_handler);

	ut_asserteq(1, run_command("ping6 " PING6_HOST, 0));
	ut_assert_nextline("Using %s device", PING6_ETHACT);
	ut_assert_nextline_empty();
	ut_assert_nextline(
		"Neighbour discovery retry count exceeded; starting again");
	ut_assert_nextline("ping6 failed; host %s is not alive", PING6_FULL);
	ut_assert_console_end();

	ut_assertok(ping6_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_ping6_fail, UTF_CONSOLE);

/* Check the arguments the command refuses */
static int cmd_test_ping6_usage(struct unit_test_state *uts)
{
	ut_assertok(env_set("ethact", PING6_ETHACT));

	/* an address is needed */
	ut_asserteq(1, run_command("ping6", 0));
	ut_assert_skip_to_line(USAGE_LAST);

	/* it must be an IPv6 one */
	ut_asserteq(1, run_command("ping6 1.1.2.2", 0));
	ut_assert_skip_to_line(USAGE_LAST);

	ut_asserteq(1, run_command("ping6 nosuchhost", 0));
	ut_assert_skip_to_line(USAGE_LAST);

	/* only one address is accepted */
	ut_asserteq(1, run_command("ping6 " PING6_HOST " " PING6_HOST, 0));
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_console_end();

	ut_assertok(ping6_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_ping6_usage, UTF_CONSOLE);
