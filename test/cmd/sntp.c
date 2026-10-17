// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the sntp command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <dm.h>
#include <env.h>
#include <i2c.h>
#include <net.h>
#include <rtc.h>
#include <asm/eth.h>
#include <asm/test.h>
#include <net/sntp.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

/* Ethernet device used by the tests */
#define SNTP_ETHACT	"eth@10002000"

/* Address sandbox starts with, which the command sends from */
#define SNTP_OWN_IP	"192.0.2.1"

/* Address of the emulated time server */
#define SNTP_SERVER	"192.0.2.2"

/* Time the emulated server hands out, in seconds since 1900 */
#define SNTP_NTP_TIME	4007188830UL

/* The same moment, as the command prints it */
#define SNTP_DATE	"Date: 2026-12-25 Time: 12:00:30"

/* ...and as the date command prints it */
#define SNTP_DATE_OUT	"Date: 2026-12-25 (Friday)    Time: 12:00:30"

/**
 * struct sntp_ctx - state shared with the transmit handler
 *
 * @uts: test state, used by the ut_assert macros in the handler
 * @requests: number of client requests the server has answered
 */
struct sntp_ctx {
	struct unit_test_state *uts;
	int requests;
};

/**
 * sb_sntp_handler() - act as a time server for the sandbox Ethernet device
 *
 * This answers the ARP request for the server and then the NTP request sent to
 * it, always with the same time.
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * @len: length of @packet in bytes
 * Return: 0 if a reply was queued, -ve on error
 */
static int sb_sntp_handler(struct udevice *dev, void *packet, unsigned int len)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct sntp_ctx *ctx = priv->priv;
	struct ethernet_hdr *eth = packet, *eth_recv;
	struct ip_udp_hdr *ip = packet + ETHER_HDR_SIZE, *ip_recv;
	struct sntp_pkt_t *reply;
	u32 seconds;

	if (ntohs(eth->et_protlen) == PROT_ARP)
		return sandbox_eth_arp_req_to_reply(dev, packet, len);

	if (ntohs(eth->et_protlen) != PROT_IP || ip->ip_p != IPPROTO_UDP ||
	    ntohs(ip->udp_dst) != NTP_SERVICE_PORT)
		return -EPROTONOSUPPORT;

	/* Don't allow the buffer to overrun */
	if (priv->recv_packets >= PKTBUFSRX)
		return 0;

	eth_recv = (void *)priv->recv_packet_buffer[priv->recv_packets];
	memcpy(eth_recv->et_dest, eth->et_src, ARP_HLEN);
	memcpy(eth_recv->et_src, priv->fake_host_hwaddr, ARP_HLEN);
	eth_recv->et_protlen = htons(PROT_IP);

	ip_recv = (void *)eth_recv + ETHER_HDR_SIZE;
	reply = (void *)ip_recv + IP_UDP_HDR_SIZE;

	memset(reply, '\0', SNTP_PACKET_LEN);
	reply->li = NTP_LI_NOLEAP;
	reply->vn = NTP_VERSION;
	reply->mode = NTP_MODE_SERVER;
	reply->stratum = 1;
	seconds = htonl(SNTP_NTP_TIME);
	memcpy(&reply->transmit_timestamp, &seconds, sizeof(seconds));

	ip_recv->ip_hl_v = 0x45;
	ip_recv->ip_tos = 0;
	ip_recv->ip_len = htons(IP_UDP_HDR_SIZE + SNTP_PACKET_LEN);
	ip_recv->ip_id = htons(1);
	ip_recv->ip_off = htons(IP_FLAGS_DFRAG);
	ip_recv->ip_ttl = 64;
	ip_recv->ip_p = IPPROTO_UDP;
	ip_recv->ip_sum = 0;
	ip_recv->ip_src = ip->ip_dst;
	ip_recv->ip_dst = ip->ip_src;
	ip_recv->ip_sum = compute_ip_checksum(ip_recv, IP_HDR_SIZE);
	ip_recv->udp_src = ip->udp_dst;
	ip_recv->udp_dst = ip->udp_src;
	ip_recv->udp_len = htons(UDP_HDR_SIZE + SNTP_PACKET_LEN);
	ip_recv->udp_xsum = 0;

	priv->recv_packet_length[priv->recv_packets] =
		ETHER_HDR_SIZE + IP_UDP_HDR_SIZE + SNTP_PACKET_LEN;
	++priv->recv_packets;
	++ctx->requests;

	return 0;
}

/**
 * sntp_setup() - point the command at the emulated server and stop the clock
 *
 * The real-time clock follows the host clock, so a second can tick between the
 * command setting the time and the test reading it back. The clock is stopped
 * until sntp_restore() puts back the time found here, since setting it changes
 * sandbox state which outlives the test.
 *
 * @uts: test state
 * @ctx: handler state, which must live until sntp_restore() is called
 * @devp: returns the real-time clock the command sets
 * @emulp: returns the emulator behind it
 * @timep: returns the time read from it
 * Return: 0 if OK, 1 on failure
 */
static int sntp_setup(struct unit_test_state *uts, struct sntp_ctx *ctx,
		      struct udevice **devp, struct udevice **emulp,
		      struct rtc_time *timep)
{
	ut_assertok(uclass_get_device(UCLASS_RTC, 0, devp));
	ut_assertok(dm_rtc_get(*devp, timep));
	ut_assertok(i2c_emul_find(*devp, emulp));
	sandbox_i2c_rtc_set_offset(*emulp, false, 0);

	sandbox_eth_set_tx_handler(0, sb_sntp_handler);
	sandbox_eth_set_priv(0, ctx);
	ut_assertok(env_set("ethact", SNTP_ETHACT));

	/*
	 * The setenv command is used rather than env_set(), since on_ipaddr()
	 * ignores a programmatic write and so net_ip would keep its old value
	 */
	ut_assertok(run_command("setenv ipaddr " SNTP_OWN_IP, 0));

	/*
	 * Under test.py the environment comes from a file which earlier runs
	 * have written to, so neither variable can be assumed to be unset
	 */
	ut_assertok(env_set("ntpserverip", NULL));
	ut_assertok(env_set("timeoffset", NULL));

	return 0;
}

/**
 * sntp_restore() - put back the clock, the environment and the handler
 *
 * @uts: test state
 * @dev: real-time clock from sntp_setup()
 * @emul: emulator from sntp_setup()
 * @time: time from sntp_setup()
 * Return: 0 if OK, 1 on failure
 */
static int sntp_restore(struct unit_test_state *uts, struct udevice *dev,
			struct udevice *emul, struct rtc_time *time)
{
	sandbox_eth_set_tx_handler(0, NULL);
	sandbox_eth_set_priv(0, NULL);
	ut_assertok(env_set("ethact", NULL));
	ut_assertok(env_set("ntpserverip", NULL));
	ut_assertok(env_set("timeoffset", NULL));

	sandbox_i2c_rtc_set_offset(emul, true, -1);
	ut_assertok(dm_rtc_set(dev, time));

	return 0;
}

/* Get the time from a server named on the command line */
static int cmd_test_sntp_base(struct unit_test_state *uts)
{
	struct sntp_ctx ctx = { .uts = uts };
	struct udevice *dev, *emul;
	struct rtc_time old;

	ut_assertok(sntp_setup(uts, &ctx, &dev, &emul, &old));

	ut_assertok(run_command("sntp " SNTP_SERVER, 0));
	ut_assert_nextline(SNTP_DATE);
	ut_assert_console_end();
	ut_asserteq(1, ctx.requests);

	/* the clock really is set, so the date command reads the same time */
	ut_assertok(run_command("date", 0));
	ut_assert_nextline(SNTP_DATE_OUT);
	ut_assert_console_end();

	ut_assertok(sntp_restore(uts, dev, emul, &old));

	return 0;
}
CMD_TEST(cmd_test_sntp_base, UTF_CONSOLE);

/* With no server on the command line the ntpserverip variable is used */
static int cmd_test_sntp_env(struct unit_test_state *uts)
{
	struct sntp_ctx ctx = { .uts = uts };
	struct udevice *dev, *emul;
	struct rtc_time old;

	ut_assertok(sntp_setup(uts, &ctx, &dev, &emul, &old));

	ut_assertok(run_command("setenv ntpserverip " SNTP_SERVER, 0));
	ut_assertok(run_command("sntp", 0));
	ut_assert_nextline(SNTP_DATE);
	ut_assert_console_end();
	ut_asserteq(1, ctx.requests);

	ut_assertok(sntp_restore(uts, dev, emul, &old));

	return 0;
}
CMD_TEST(cmd_test_sntp_env, UTF_CONSOLE);

/* The timeoffset variable moves the clock away from UTC */
static int cmd_test_sntp_offset(struct unit_test_state *uts)
{
	struct sntp_ctx ctx = { .uts = uts };
	struct udevice *dev, *emul;
	struct rtc_time old;

	ut_assertok(sntp_setup(uts, &ctx, &dev, &emul, &old));

	/* two hours on */
	ut_assertok(env_set("timeoffset", "7200"));
	ut_assertok(run_command("sntp " SNTP_SERVER, 0));
	ut_assert_nextline("Date: 2026-12-25 Time: 14:00:30");
	ut_assert_console_end();

	/* and half an hour back */
	ut_assertok(env_set("timeoffset", "-1800"));
	ut_assertok(run_command("sntp " SNTP_SERVER, 0));
	ut_assert_nextline("Date: 2026-12-25 Time: 11:30:30");
	ut_assert_console_end();

	ut_assertok(sntp_restore(uts, dev, emul, &old));

	return 0;
}
CMD_TEST(cmd_test_sntp_offset, UTF_CONSOLE);

/* The command needs a server to ask */
static int cmd_test_sntp_noserver(struct unit_test_state *uts)
{
	struct sntp_ctx ctx = { .uts = uts };
	struct udevice *dev, *emul;
	struct rtc_time old;

	ut_assertok(sntp_setup(uts, &ctx, &dev, &emul, &old));

	/* with nothing to go on the command gives up before sending anything */
	ut_asserteq(1, run_command("sntp", 0));
	ut_assert_nextline("ntpserverip not set");
	ut_assert_console_end();

	/* an address which cannot be parsed is refused as well */
	ut_asserteq(1, run_command("sntp not-an-address", 0));
	ut_assert_nextline("Bad NTP server IP address");
	ut_assert_console_end();

	/* the same goes for one which parses as the unspecified address */
	ut_assertok(run_command("setenv ntpserverip 0.0.0.0", 0));
	ut_asserteq(1, run_command("sntp", 0));
	ut_assert_nextline("ntpserverip not set");
	ut_assert_console_end();

	ut_asserteq(0, ctx.requests);

	ut_assertok(sntp_restore(uts, dev, emul, &old));

	return 0;
}
CMD_TEST(cmd_test_sntp_noserver, UTF_CONSOLE);

/* The command takes at most one argument */
static int cmd_test_sntp_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("sntp " SNTP_SERVER " extra", 0));
	ut_assert_nextline("sntp - synchronize RTC via network");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("sntp [NTP server IP]");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_sntp_usage, UTF_CONSOLE);
