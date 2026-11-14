// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the ping command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <env.h>
#include <net.h>
#include <asm/eth.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

/* Ethernet device the tests use, and its sequence number */
#define PING_ETHACT	"eth@10002000"
#define PING_SEQ	0

/* Host the tests ping, which the sandbox driver answers for */
#define PING_HOST	"1.1.2.2"

/* Last line of the help text, which ends the usage message */
#define USAGE_LAST	"ping pingAddress"

/**
 * ping_restore() - put back the device and the retry setting
 *
 * The setenv command is used for netretry, since on_netretry() ignores a
 * programmatic write.
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int ping_restore(struct unit_test_state *uts)
{
	sandbox_eth_disable_response(PING_SEQ, false);
	ut_assertok(env_set("ethact", NULL));
	ut_assertok(run_command("setenv netretry", 0));

	return 0;
}

/* Ping a host which answers */
static int cmd_test_ping_base(struct unit_test_state *uts)
{
	ut_assertok(env_set("ethact", PING_ETHACT));

	ut_assertok(run_command("ping " PING_HOST, 0));
	ut_assert_nextline("Using %s device", PING_ETHACT);
	ut_assert_nextline("host %s is alive", PING_HOST);
	ut_assert_console_end();

	ut_assertok(ping_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_ping_base, UTF_CONSOLE);

/* Ping a host which does not answer */
static int cmd_test_ping_fail(struct unit_test_state *uts)
{
	ut_assertok(env_set("ethact", PING_ETHACT));

	/*
	 * With the emulated host silent the command waits ten seconds for a
	 * reply, so move the sandbox timer past that rather than sitting the
	 * wait out. One jump is enough, since netretry stops the network loop
	 * from starting again.
	 */
	sandbox_eth_disable_response(PING_SEQ, true);
	ut_assertok(run_command("setenv netretry no", 0));
	sandbox_eth_skip_timeout();

	ut_asserteq(1, run_command("ping " PING_HOST, 0));
	ut_assert_nextline("Using %s device", PING_ETHACT);
	ut_assert_nextline("ping failed; host %s is not alive", PING_HOST);
	ut_assert_console_end();

	/*
	 * The network loop leaves the ARP request outstanding when it gives up,
	 * so a later test which uses the network sees the retry. Let the
	 * emulated host answer again and ping it, which clears the request.
	 */
	ut_assert(arp_is_waiting());
	sandbox_eth_disable_response(PING_SEQ, false);
	ut_assertok(run_command("ping " PING_HOST, 0));
	ut_assert_nextline("Using %s device", PING_ETHACT);
	ut_assert_nextline("host %s is alive", PING_HOST);
	ut_assert_console_end();
	ut_assert(!arp_is_waiting());

	ut_assertok(ping_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_ping_fail, UTF_CONSOLE);

/* Check the arguments the command refuses */
static int cmd_test_ping_usage(struct unit_test_state *uts)
{
	ut_assertok(env_set("ethact", PING_ETHACT));

	/* an address is needed */
	ut_asserteq(1, run_command("ping", 0));
	ut_assert_skip_to_line(USAGE_LAST);

	/* so is one which parses */
	ut_asserteq(1, run_command("ping nosuchhost", 0));
	ut_assert_skip_to_line(USAGE_LAST);

	/* 0.0.0.0 cannot be told apart from an address which does not parse */
	ut_asserteq(1, run_command("ping 0.0.0.0", 0));
	ut_assert_skip_to_line(USAGE_LAST);

	/* only one address is accepted */
	ut_asserteq(1, run_command("ping " PING_HOST " " PING_HOST, 0));
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_console_end();

	ut_assertok(ping_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_ping_usage, UTF_CONSOLE);
