// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the dhcp6 command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <dm.h>
#include <env.h>
#include <mapmem.h>
#include <net.h>
#include <net6.h>
#include <time.h>
#include <asm/eth.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

/* Ethernet device the tests use, and its sequence number */
#define DHCP6_ETHACT	"eth@10002000"
#define DHCP6_SEQ	0

/* Address, prefix length and router the tests give the board to start with */
#define DHCP6_OWN_ADDR	"2001:db8::1/64"
#define DHCP6_ROUTER	"2001:db8::ff"

/* Address the emulated server hands out, and the one it answers from */
#define DHCP6_CLIENT	"2001:db8::20"
#define DHCP6_SERVER	"2001:db8::2"

/* The same two as U-Boot shows them, since %pI6c does not compress */
#define DHCP6_CLIENT_FULL	"2001:0db8:0000:0000:0000:0000:0000:0020"
#define DHCP6_SERVER_FULL	"2001:0db8:0000:0000:0000:0000:0000:0002"

/* Address and prefix length the command leaves in the environment */
#define DHCP6_BOUND	DHCP6_CLIENT_FULL "/64"

/* Boot file the server offers, as a URL and as the name TFTP asks for */
#define DHCP6_URL	"tftp://[" DHCP6_SERVER "]/boot.img"
#define DHCP6_FILE	"boot.img"

/* Address the tests load the file to */
#define DHCP6_LOAD_ADDR	0x1000000

/* Contents of that file, and its length */
#define DHCP6_DATA	"U-Boot dhcp6 test file\n"
#define DHCP6_SIZE	((int)sizeof(DHCP6_DATA) - 1)

/* Lifetimes the server offers, in seconds, which the client stores unused */
#define DHCP6_T1	1800
#define DHCP6_T2	2880
#define DHCP6_PREFERRED	3600
#define DHCP6_VALID	7200

/* Ports the client and the server listen on */
#define PORT_SERVER	547
#define PORT_CLIENT	546

/* DHCPv6 message types, from RFC 8415 */
#define MSG_SOLICIT	1
#define MSG_ADVERTISE	2
#define MSG_REQUEST	3
#define MSG_REPLY	7

/* DHCPv6 options the emulated server reads and writes */
#define OPT_CLIENTID		1
#define OPT_SERVERID		2
#define OPT_IA_NA		3
#define OPT_IAADDR		5
#define OPT_PREFERENCE		7
#define OPT_BOOTFILE_URL	59

/* Size of a DHCPv6 message header and of an option header */
#define MSG_HDR_LEN	4
#define OPT_HDR_LEN	4

/* DUID-LL: a type, a hardware type and a MAC address, from RFC 8415 sec 11.2 */
#define DUID_TYPE_LL	3
#define DUID_HW_ENET	1
#define DUID_LEN	(4 + ARP_HLEN)

/* An IAADDR holds an address and two lifetimes */
#define IAADDR_LEN	(IN6ADDRSZ + 8)

/* An IA_NA holds an IA_ID, T1, T2 and, here, one IAADDR option */
#define IA_NA_LEN	(12 + OPT_HDR_LEN + IAADDR_LEN)

/* Well-known TFTP port */
#define TFTP_PORT	69

/* Port the emulated server transfers from, chosen at random */
#define TFTP_TID	21314

/* TFTP operations */
#define TFTP_RRQ	1
#define TFTP_DATA	3
#define TFTP_ACK	4

/* Time to move the sandbox clock on by for each idle poll, in ms */
#define DHCP6_JUMP	20000

/* Second line of the help message, which is part of the usage message */
#define DHCP6_HELP2	"Use IPv6 hostIPaddr framed with [] brackets"

struct tftp_hdr {
	u16 opcode;
	u16 block;
};

#define TFTP_HDR_SIZE	sizeof(struct tftp_hdr)

/**
 * struct dhcp6_ctx - state shared with the transmit handler
 *
 * @uts: test state, used by the ut_assert macros in the handler
 * @fname: name the client asked the TFTP server for
 * @advertises: number of ADVERTISE messages sent
 * @replies: number of REPLY messages sent
 */
struct dhcp6_ctx {
	struct unit_test_state *uts;
	char fname[128];
	int advertises;
	int replies;
};

/**
 * put_long() - store a 32-bit value in network byte order
 *
 * @p: position to store it at
 * @val: value to store
 */
static void put_long(uchar *p, u32 val)
{
	val = htonl(val);
	memcpy(p, &val, sizeof(val));
}

/**
 * find_opt() - find an option in a DHCPv6 message
 *
 * @msg: start of the message, at its header
 * @mlen: length of the message in bytes
 * @id: option number to look for
 * @lenp: returns the length of the option's data, on success
 * Return: pointer to the option's data, or NULL if it is not there
 */
static uchar *find_opt(uchar *msg, int mlen, int id, int *lenp)
{
	uchar *p = msg + MSG_HDR_LEN;

	while (p + OPT_HDR_LEN <= msg + mlen) {
		int olen = (p[2] << 8) | p[3];

		if (((p[0] << 8) | p[1]) == id) {
			*lenp = olen;
			return p + OPT_HDR_LEN;
		}
		p += OPT_HDR_LEN + olen;
	}

	return NULL;
}

/**
 * add_opt() - add an option, with its header, to a message
 *
 * @p: position in the message, updated on return
 * @id: option number
 * @data: option data, which may be NULL for an option of no length
 * @len: number of bytes of data
 */
static void add_opt(uchar **p, int id, const void *data, int len)
{
	uchar *ptr = *p;

	*ptr++ = id >> 8;
	*ptr++ = id & 0xff;
	*ptr++ = len >> 8;
	*ptr++ = len & 0xff;
	if (len)
		memcpy(ptr, data, len);
	*p = ptr + len;
}

/**
 * add_duid() - add a DUID-LL option holding a MAC address
 *
 * @p: position in the message, updated on return
 * @id: option number, CLIENTID or SERVERID
 * @mac: hardware address the DUID names
 */
static void add_duid(uchar **p, int id, const uchar *mac)
{
	uchar duid[DUID_LEN];

	duid[0] = 0;
	duid[1] = DUID_TYPE_LL;
	duid[2] = 0;
	duid[3] = DUID_HW_ENET;
	memcpy(duid + 4, mac, ARP_HLEN);
	add_opt(p, id, duid, sizeof(duid));
}

/**
 * add_ia_na() - add an IA_NA option offering the client an address
 *
 * The IA_ID is the one the client chose, since a reply naming a different
 * one is thrown away by dhcp6_parse_options().
 *
 * @p: position in the message, updated on return
 * @iaid: the four bytes of the client's IA_ID
 */
static void add_ia_na(uchar **p, const uchar *iaid)
{
	uchar ia[IA_NA_LEN], iaaddr[IAADDR_LEN], *q = ia;
	struct in6_addr addr;

	string_to_ip6(DHCP6_CLIENT, strlen(DHCP6_CLIENT), &addr);
	memcpy(iaaddr, &addr, IN6ADDRSZ);
	put_long(iaaddr + IN6ADDRSZ, DHCP6_PREFERRED);
	put_long(iaaddr + IN6ADDRSZ + 4, DHCP6_VALID);

	memcpy(q, iaid, sizeof(u32));
	q += sizeof(u32);
	put_long(q, DHCP6_T1);
	q += sizeof(u32);
	put_long(q, DHCP6_T2);
	q += sizeof(u32);
	add_opt(&q, OPT_IAADDR, iaaddr, sizeof(iaaddr));

	add_opt(p, OPT_IA_NA, ia, q - ia);
}

/**
 * dhcp6_req_to_reply() - answer a SOLICIT or a REQUEST
 *
 * A SOLICIT is answered with an ADVERTISE and a REQUEST with a REPLY, both
 * carrying the same options: the client's own CLIENTID, a SERVERID, the
 * address offered inside the client's IA_NA, the highest preference so that
 * the client acts on the first advertise rather than waiting out the solicit
 * period, and the URL of the boot file.
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * @len: length of @packet in bytes
 * Return: 0 if a reply was queued, -EAGAIN if the packet is not a request
 */
static int dhcp6_req_to_reply(struct udevice *dev, void *packet,
			      unsigned int len)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct dhcp6_ctx *ctx = priv->priv;
	struct ethernet_hdr *eth = packet, *eth_recv;
	struct ip6_hdr *ip6 = packet + ETHER_HDR_SIZE, *ip6_recv;
	struct udp_hdr *udp, *udp_recv;
	uchar *msg, *reply, *p, *cid, *iaid;
	int mlen, cidlen, ialen, type, udplen;
	uchar pref = 255;

	if (ntohs(eth->et_protlen) != PROT_IP6 || ip6->nexthdr != IPPROTO_UDP)
		return -EAGAIN;

	udp = (void *)ip6 + IP6_HDR_SIZE;
	if (ntohs(udp->udp_dst) != PORT_SERVER)
		return -EAGAIN;

	msg = (uchar *)udp + UDP_HDR_SIZE;
	mlen = ntohs(udp->udp_len) - UDP_HDR_SIZE;
	type = msg[0];
	if (type != MSG_SOLICIT && type != MSG_REQUEST)
		return -EAGAIN;

	cid = find_opt(msg, mlen, OPT_CLIENTID, &cidlen);
	iaid = find_opt(msg, mlen, OPT_IA_NA, &ialen);
	if (!cid || !iaid || ialen < sizeof(u32))
		return -EAGAIN;

	/* Don't allow the buffer to overrun */
	if (priv->recv_packets >= PKTBUFSRX)
		return 0;

	eth_recv = (void *)priv->recv_packet_buffer[priv->recv_packets];
	memcpy(eth_recv->et_dest, eth->et_src, ARP_HLEN);
	memcpy(eth_recv->et_src, priv->fake_host_hwaddr, ARP_HLEN);
	eth_recv->et_protlen = htons(PROT_IP6);

	ip6_recv = (void *)eth_recv + ETHER_HDR_SIZE;
	memset(ip6_recv, 0, IP6_HDR_SIZE);
	ip6_recv->version = 6;
	ip6_recv->nexthdr = IPPROTO_UDP;
	ip6_recv->hop_limit = 255;
	string_to_ip6(DHCP6_SERVER, strlen(DHCP6_SERVER), &ip6_recv->saddr);
	net_copy_ip6(&ip6_recv->daddr, &ip6->saddr);

	udp_recv = (void *)ip6_recv + IP6_HDR_SIZE;
	udp_recv->udp_src = htons(PORT_SERVER);
	udp_recv->udp_dst = htons(PORT_CLIENT);

	reply = (uchar *)udp_recv + UDP_HDR_SIZE;
	p = reply;

	/* keep the transaction ID of the request and answer it */
	memcpy(p, msg, MSG_HDR_LEN);
	p[0] = type == MSG_SOLICIT ? MSG_ADVERTISE : MSG_REPLY;
	p += MSG_HDR_LEN;

	add_opt(&p, OPT_CLIENTID, cid, cidlen);
	add_duid(&p, OPT_SERVERID, priv->fake_host_hwaddr);
	add_ia_na(&p, iaid);
	add_opt(&p, OPT_PREFERENCE, &pref, sizeof(pref));
	add_opt(&p, OPT_BOOTFILE_URL, DHCP6_URL, strlen(DHCP6_URL));

	udplen = UDP_HDR_SIZE + (p - reply);
	ip6_recv->payload_len = htons(udplen);
	udp_recv->udp_len = htons(udplen);
	udp_recv->udp_xsum = 0;
	udp_recv->udp_xsum =
		csum_ipv6_magic(&ip6_recv->saddr, &ip6_recv->daddr, udplen,
				IPPROTO_UDP,
				csum_partial((u8 *)udp_recv, udplen, 0));

	priv->recv_packet_length[priv->recv_packets] =
		ETHER_HDR_SIZE + IP6_HDR_SIZE + udplen;
	++priv->recv_packets;
	if (type == MSG_SOLICIT)
		++ctx->advertises;
	else
		++ctx->replies;

	return 0;
}

/**
 * tftp6_req_to_reply() - serve the boot file to the client over IPv6
 *
 * The options in the request are ignored, so the block size is the default of
 * 512 bytes and the whole file goes out in the first block. The ACK which
 * comes back ends the transfer and needs no answer.
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * @len: length of @packet in bytes
 * Return: 0 if a reply was queued, -EAGAIN if the packet is not a request
 */
static int tftp6_req_to_reply(struct udevice *dev, void *packet,
			      unsigned int len)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct dhcp6_ctx *ctx = priv->priv;
	struct ethernet_hdr *eth = packet, *eth_recv;
	struct ip6_hdr *ip6 = packet + ETHER_HDR_SIZE, *ip6_recv;
	struct udp_hdr *udp, *udp_recv;
	struct tftp_hdr *tftp, *tftp_recv;
	int size, udplen;

	if (ntohs(eth->et_protlen) != PROT_IP6 || ip6->nexthdr != IPPROTO_UDP)
		return -EAGAIN;

	udp = (void *)ip6 + IP6_HDR_SIZE;
	tftp = (void *)udp + UDP_HDR_SIZE;
	if (ntohs(udp->udp_dst) == TFTP_PORT) {
		if (ntohs(tftp->opcode) != TFTP_RRQ)
			return -EAGAIN;
		strlcpy(ctx->fname, (char *)tftp + sizeof(u16),
			sizeof(ctx->fname));
	} else if (ntohs(udp->udp_dst) == TFTP_TID) {
		if (ntohs(tftp->opcode) != TFTP_ACK)
			return -EAGAIN;

		/* The transfer is over, since the file fits in one block */
		return 0;
	} else {
		return -EAGAIN;
	}

	/* Don't allow the buffer to overrun */
	if (priv->recv_packets >= PKTBUFSRX)
		return 0;

	size = DHCP6_SIZE;
	eth_recv = (void *)priv->recv_packet_buffer[priv->recv_packets];
	memcpy(eth_recv->et_dest, eth->et_src, ARP_HLEN);
	memcpy(eth_recv->et_src, priv->fake_host_hwaddr, ARP_HLEN);
	eth_recv->et_protlen = htons(PROT_IP6);

	ip6_recv = (void *)eth_recv + ETHER_HDR_SIZE;
	memset(ip6_recv, 0, IP6_HDR_SIZE);
	ip6_recv->version = 6;
	ip6_recv->nexthdr = IPPROTO_UDP;
	ip6_recv->hop_limit = 64;
	net_copy_ip6(&ip6_recv->saddr, &ip6->daddr);
	net_copy_ip6(&ip6_recv->daddr, &ip6->saddr);

	udp_recv = (void *)ip6_recv + IP6_HDR_SIZE;
	udp_recv->udp_src = htons(TFTP_TID);
	udp_recv->udp_dst = udp->udp_src;

	tftp_recv = (void *)udp_recv + UDP_HDR_SIZE;
	tftp_recv->opcode = htons(TFTP_DATA);
	tftp_recv->block = htons(1);
	memcpy((void *)tftp_recv + TFTP_HDR_SIZE, DHCP6_DATA, size);

	udplen = UDP_HDR_SIZE + TFTP_HDR_SIZE + size;
	ip6_recv->payload_len = htons(udplen);
	udp_recv->udp_len = htons(udplen);
	udp_recv->udp_xsum = 0;
	udp_recv->udp_xsum =
		csum_ipv6_magic(&ip6_recv->saddr, &ip6_recv->daddr, udplen,
				IPPROTO_UDP,
				csum_partial((u8 *)udp_recv, udplen, 0));

	priv->recv_packet_length[priv->recv_packets] =
		ETHER_HDR_SIZE + IP6_HDR_SIZE + udplen;
	++priv->recv_packets;

	return 0;
}

/**
 * sb_dhcp6_handler() - act as a DHCPv6 and TFTP server
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * @len: length of @packet in bytes
 * Return: 0, whether or not a reply was queued
 */
static int sb_dhcp6_handler(struct udevice *dev, void *packet,
			    unsigned int len)
{
	if (!sandbox_eth_ndisc_req_to_reply(dev, packet, len))
		return 0;
	if (!dhcp6_req_to_reply(dev, packet, len))
		return 0;
	if (!tftp6_req_to_reply(dev, packet, len))
		return 0;

	return 0;
}

/**
 * sb_dhcp6_rx() - move the sandbox clock on for each idle poll
 *
 * With nothing answering, the command sends five solicits with a doubling
 * interval before giving up, which is 31 seconds of real time. The clock is
 * moved past the timeout as U-Boot looks for a reply, rather than as it
 * sends, since the state machine starts the timer only after the packet has
 * gone out; that brings the test down to a few milliseconds.
 *
 * @dev: sandbox Ethernet device
 * Return: -EAGAIN, since no packet is ever queued
 */
static int sb_dhcp6_rx(struct udevice *dev)
{
	timer_test_add_offset(DHCP6_JUMP);

	return -EAGAIN;
}

/**
 * dhcp6_setup() - point the dhcp6 command at the emulated server
 *
 * The board is given an address and a router so that the network loop does
 * not go looking for one with a router solicitation. That search happens on
 * the first IPv6 command alone, so a test which let it run would behave
 * differently on its own and in a suite.
 *
 * The setenv command is used for the addresses, since on_ip6addr() and
 * on_gatewayip6() ignore a programmatic write. Delays are skipped because the
 * state machine waits a random time of up to a second before the first
 * solicit.
 *
 * @uts: test state
 * @ctx: handler state, which must live until dhcp6_restore() is called
 * Return: 0 if OK, 1 on failure
 */
static int dhcp6_setup(struct unit_test_state *uts, struct dhcp6_ctx *ctx)
{
	ut_set_skip_delays(uts, true);
	ut_assertok(env_set("ethact", DHCP6_ETHACT));
	ut_assertok(run_command("setenv netretry no", 0));
	ut_assertok(run_command("setenv ip6addr " DHCP6_OWN_ADDR, 0));
	ut_assertok(run_command("setenv gatewayip6 " DHCP6_ROUTER, 0));
	if (ctx) {
		sandbox_eth_set_tx_handler(DHCP6_SEQ, sb_dhcp6_handler);
		sandbox_eth_set_priv(DHCP6_SEQ, ctx);
	}

	return 0;
}

/**
 * dhcp6_restore() - put back the environment and the sandbox device
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int dhcp6_restore(struct unit_test_state *uts)
{
	sandbox_eth_set_tx_handler(DHCP6_SEQ, NULL);
	sandbox_eth_set_rx_handler(DHCP6_SEQ, NULL);
	sandbox_eth_set_priv(DHCP6_SEQ, NULL);
	ut_assertok(env_set("ethact", NULL));
	ut_assertok(run_command("setenv netretry", 0));
	ut_assertok(run_command("setenv ip6addr", 0));
	ut_assertok(run_command("setenv gatewayip6", 0));
	ut_assertok(run_command("setenv serverip6", 0));
	ut_assertok(env_set("autoload", NULL));
	ut_assertok(env_set("filesize", NULL));
	ut_assertok(env_set("fileaddr", NULL));

	return 0;
}

/**
 * check_lease() - check the exchange and the settings it leaves behind
 *
 * @uts: test state
 * @ctx: handler state
 * Return: 0 if OK, 1 on failure
 */
static int check_lease(struct unit_test_state *uts, struct dhcp6_ctx *ctx)
{
	/* the lease takes one advertise and one reply */
	ut_asserteq(1, ctx->advertises);
	ut_asserteq(1, ctx->replies);

	ut_asserteq_str(DHCP6_BOUND, env_get("ip6addr"));
	ut_asserteq_str(DHCP6_SERVER_FULL, env_get("serverip6"));

	return 0;
}

/* Obtain a lease and stop there, with autoload turned off */
static int cmd_test_dhcp6_base(struct unit_test_state *uts)
{
	struct dhcp6_ctx ctx = { .uts = uts };

	ut_assertok(dhcp6_setup(uts, &ctx));
	ut_assertok(env_set("autoload", "no"));

	ut_assertok(run_command("dhcp6", 0));
	ut_assert_nextline("DHCP6 SOLICIT 0");
	ut_assert_nextline("DHCP6 REQUEST 0");
	ut_assert_nextline("DHCP6 client bound to " DHCP6_CLIENT_FULL);
	ut_assert_console_end();

	ut_assertok(check_lease(uts, &ctx));

	/* Nothing was asked of the TFTP server */
	ut_asserteq_str("", ctx.fname);

	ut_assertok(dhcp6_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_dhcp6_base, UTF_CONSOLE);

/* Obtain a lease and download the boot file the server names */
static int cmd_test_dhcp6_load(struct unit_test_state *uts)
{
	struct dhcp6_ctx ctx = { .uts = uts };

	ut_assertok(dhcp6_setup(uts, &ctx));

	ut_assertok(run_commandf("dhcp6 %x", DHCP6_LOAD_ADDR));
	ut_assert_nextline("DHCP6 SOLICIT 0");
	ut_assert_nextline("DHCP6 REQUEST 0");
	ut_assert_nextline("DHCP6 client bound to " DHCP6_CLIENT_FULL);
	ut_assert_nextline("Using %s device", DHCP6_ETHACT);
	ut_assert_nextline("TFTP from server " DHCP6_SERVER_FULL
			   "; our IP address is " DHCP6_CLIENT_FULL);
	ut_assert_nextline("Filename '%s'.", DHCP6_FILE);
	ut_assert_nextline("Load address: 0x%x", DHCP6_LOAD_ADDR);
	/*
	 * The transfer rate is reported only when the transfer takes a
	 * millisecond or more, which a file this small does not always manage
	 */
	ut_assert_nextlinen("Loading: ");
	ut_assert_skip_to_line("done");
	ut_assert_nextline("Bytes transferred = %d (%x hex)", DHCP6_SIZE,
			   DHCP6_SIZE);
	ut_assert_console_end();

	ut_assertok(check_lease(uts, &ctx));
	ut_asserteq_str(DHCP6_FILE, ctx.fname);
	ut_asserteq(DHCP6_SIZE, env_get_hex("filesize", 0));
	ut_asserteq_mem(DHCP6_DATA, map_sysmem(DHCP6_LOAD_ADDR, 0),
			DHCP6_SIZE);

	ut_assertok(dhcp6_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_dhcp6_load, UTF_CONSOLE);

/*
 * A file named on the command line is overridden by the one the server
 * offers, since dhcp6_parse_options() writes the boot file URL over it
 */
static int cmd_test_dhcp6_file(struct unit_test_state *uts)
{
	struct dhcp6_ctx ctx = { .uts = uts };

	ut_assertok(dhcp6_setup(uts, &ctx));

	ut_assertok(run_commandf("dhcp6 %x other.img", DHCP6_LOAD_ADDR));
	ut_assert_skip_to_line("Filename '%s'.", DHCP6_FILE);
	ut_assert_skip_to_line("Bytes transferred = %d (%x hex)", DHCP6_SIZE,
			       DHCP6_SIZE);
	ut_assert_console_end();

	ut_asserteq_str(DHCP6_FILE, ctx.fname);

	ut_assertok(dhcp6_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_dhcp6_file, UTF_CONSOLE);

/* With nothing answering, the solicit is retried until the command gives up */
static int cmd_test_dhcp6_fail(struct unit_test_state *uts)
{
	int i;

	ut_assertok(dhcp6_setup(uts, NULL));
	sandbox_eth_set_rx_handler(DHCP6_SEQ, sb_dhcp6_rx);

	ut_asserteq(1, run_command("dhcp6", 0));
	for (i = 0; i < CONFIG_NET_RETRY_COUNT; i++)
		ut_assert_nextline("DHCP6 SOLICIT %d", i);
	ut_assert_nextline("DHCP6 FAILED, TERMINATING");
	ut_assert_console_end();

	/* no address was obtained, so the environment is left alone */
	ut_asserteq_str(DHCP6_OWN_ADDR, env_get("ip6addr"));

	ut_assertok(dhcp6_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_dhcp6_fail, UTF_CONSOLE);

/**
 * check_usage() - check the usage message the command shows
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline(
		"dhcp6 - boot image via network using DHCPv6/TFTP protocol.");
	ut_assert_nextline(DHCP6_HELP2);
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("dhcp6 [loadAddress] [[hostIPaddr:]bootfilename]");
	ut_assert_console_end();

	return 0;
}

/* The command takes at most two arguments */
static int cmd_test_dhcp6_usage(struct unit_test_state *uts)
{
	ut_assertok(dhcp6_setup(uts, NULL));

	ut_asserteq(1, run_command("dhcp6 1000 boot.img extra", 0));
	ut_assertok(check_usage(uts));

	ut_assertok(dhcp6_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_dhcp6_usage, UTF_CONSOLE);

/* An option is refused rather than being taken as a file name */
static int cmd_test_dhcp6_badopt(struct unit_test_state *uts)
{
	struct dhcp6_ctx ctx = { .uts = uts };

	/* the emulated server is attached, so a command which runs would boot */
	ut_assertok(dhcp6_setup(uts, &ctx));

	ut_asserteq(1, run_command("dhcp6 -x", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(0, ctx.advertises);
	ut_asserteq_str("", ctx.fname);

	ut_assertok(dhcp6_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_dhcp6_badopt, UTF_CONSOLE);
