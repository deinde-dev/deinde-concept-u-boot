// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the dhcp command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <dm.h>
#include <env.h>
#include <mapmem.h>
#include <net.h>
#include <asm/eth.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>
#include "../../net/bootp.h"

/* Ethernet device used by the tests */
#define DHCP_ETHACT	"eth@10002000"

/* Address the emulated server hands out and the one it answers from */
#define DHCP_CLIENT_IP	"192.0.2.10"
#define DHCP_SERVER_IP	"192.0.2.2"

/* Address sandbox starts with, which the tests put back afterwards */
#define DHCP_OWN_IP	"192.0.2.1"

/* Network parameters the emulated server offers */
#define DHCP_NETMASK	"255.255.255.0"
#define DHCP_GATEWAY	"192.0.2.3"
#define DHCP_DNS	"192.0.2.4"
#define DHCP_DNS2	"192.0.2.5"
#define DHCP_HOSTNAME	"sandbox"

/* Lease time the emulated server offers, in seconds */
#define DHCP_LEASE	3600

/* Boot file the emulated server offers */
#define DHCP_FILE	"boot.img"

/* Address the tests load the file to */
#define DHCP_LOAD_ADDR	0x1000000

/* Contents of that file, and its length */
#define DHCP_DATA	"U-Boot dhcp test file\n"
#define DHCP_SIZE	((int)sizeof(DHCP_DATA) - 1)

/* Well-known TFTP port */
#define TFTP_PORT	69

/* Port the emulated server transfers from, chosen at random */
#define TFTP_TID	21314

/* TFTP operations */
#define TFTP_RRQ	1
#define TFTP_DATA	3
#define TFTP_ACK	4

/* Vendor-extension magic cookie, from RFC 1048 */
#define DHCP_MAGIC	0x63825363

/* DHCP options the emulated server includes */
#define OPT_NETMASK	1
#define OPT_GATEWAY	3
#define OPT_DNS		6
#define OPT_HOSTNAME	12
#define OPT_LEASE	51
#define OPT_MSG_TYPE	53
#define OPT_SERVERID	54
#define OPT_BOOTFILE	67
#define OPT_END		255

struct tftp_hdr {
	u16 opcode;
	u16 block;
};

#define TFTP_HDR_SIZE	sizeof(struct tftp_hdr)

/**
 * struct dhcp_ctx - state shared with the transmit handler
 *
 * @uts: test state, used by the ut_assert macros in the handler
 * @fname: name the client asked the TFTP server for
 * @offers: number of DHCPOFFER messages sent
 * @acks: number of DHCPACK messages sent
 */
struct dhcp_ctx {
	struct unit_test_state *uts;
	char fname[128];
	int offers;
	int acks;
};

/**
 * add_ip_opt() - add an option holding one or two addresses
 *
 * @p: position in the option area, updated on return
 * @tag: option number
 * @ip: address to store
 * @ip2: second address to store, or NULL for an option holding one
 */
static void add_ip_opt(uchar **p, int tag, const char *ip, const char *ip2)
{
	struct in_addr addr = string_to_ip(ip);
	int size = sizeof(addr.s_addr);
	uchar *ptr = *p;

	*ptr++ = tag;
	*ptr++ = ip2 ? size * 2 : size;
	memcpy(ptr, &addr.s_addr, size);
	ptr += size;
	if (ip2) {
		addr = string_to_ip(ip2);
		memcpy(ptr, &addr.s_addr, size);
		ptr += size;
	}
	*p = ptr;
}

/**
 * add_str_opt() - add an option holding a string
 *
 * @p: position in the option area, updated on return
 * @tag: option number
 * @str: string to store, without its terminator
 */
static void add_str_opt(uchar **p, int tag, const char *str)
{
	uchar *ptr = *p;
	int len = strlen(str);

	*ptr++ = tag;
	*ptr++ = len;
	memcpy(ptr, str, len);
	*p = ptr + len;
}

/**
 * add_byte_opt() - add an option holding a single byte
 *
 * @p: position in the option area, updated on return
 * @tag: option number
 * @val: value to store
 */
static void add_byte_opt(uchar **p, int tag, int val)
{
	uchar *ptr = *p;

	*ptr++ = tag;
	*ptr++ = 1;
	*ptr++ = val;
	*p = ptr;
}

/**
 * add_long_opt() - add an option holding a 32-bit value
 *
 * @p: position in the option area, updated on return
 * @tag: option number
 * @val: value to store
 */
static void add_long_opt(uchar **p, int tag, u32 val)
{
	uchar *ptr = *p;

	*ptr++ = tag;
	*ptr++ = sizeof(val);
	val = htonl(val);
	memcpy(ptr, &val, sizeof(val));
	*p = ptr + sizeof(val);
}

/**
 * get_msg_type() - find the message type of a DHCP packet
 *
 * @bp: packet to look at
 * @end: first byte after the packet
 * Return: message type (DHCP_DISCOVER and friends), or -1 if there is none
 */
static int get_msg_type(struct bootp_hdr *bp, const void *end)
{
	uchar *popt = (uchar *)&bp->bp_vend[4];

	if (net_read_u32((u32 *)&bp->bp_vend[0]) != htonl(DHCP_MAGIC))
		return -1;

	while ((void *)popt < end && *popt != OPT_END) {
		if (*popt == OPT_MSG_TYPE)
			return popt[2];
		popt += *popt ? popt[1] + 2 : 1;
	}

	return -1;
}

/**
 * dhcp_req_to_reply() - answer a DHCPDISCOVER or a DHCPREQUEST
 *
 * The reply repeats the request, so that the transaction ID and the hardware
 * address match, then fills in the address offered to the client, the server
 * and the boot file, along with the options a DHCP server provides. A
 * DHCPDISCOVER is answered with a DHCPOFFER and a DHCPREQUEST with a DHCPACK,
 * both carrying the same options.
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * @len: length of @packet in bytes
 * Return: 0 if a reply was queued, -EAGAIN if the packet is not a request
 */
static int dhcp_req_to_reply(struct udevice *dev, void *packet,
			     unsigned int len)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct dhcp_ctx *ctx = priv->priv;
	struct ethernet_hdr *eth = packet, *eth_recv;
	struct ip_udp_hdr *ip = packet + ETHER_HDR_SIZE, *ip_recv;
	struct bootp_hdr *bp, *bp_recv;
	int type, optlen, iplen;
	uchar *p;

	if (ntohs(eth->et_protlen) != PROT_IP || ip->ip_p != IPPROTO_UDP ||
	    ntohs(ip->udp_dst) != PORT_BOOTPS)
		return -EAGAIN;

	bp = (void *)ip + IP_UDP_HDR_SIZE;
	if (bp->bp_op != OP_BOOTREQUEST)
		return -EAGAIN;

	type = get_msg_type(bp, packet + len);
	if (type != DHCP_DISCOVER && type != DHCP_REQUEST)
		return -EAGAIN;

	/* Don't allow the buffer to overrun */
	if (priv->recv_packets >= PKTBUFSRX)
		return 0;

	eth_recv = (void *)priv->recv_packet_buffer[priv->recv_packets];
	memcpy(eth_recv, packet, len);
	memcpy(eth_recv->et_dest, eth->et_src, ARP_HLEN);
	memcpy(eth_recv->et_src, priv->fake_host_hwaddr, ARP_HLEN);

	ip_recv = (void *)eth_recv + ETHER_HDR_SIZE;
	ip_recv->ip_src = string_to_ip(DHCP_SERVER_IP);
	ip_recv->udp_src = ip->udp_dst;
	ip_recv->udp_dst = ip->udp_src;
	ip_recv->udp_xsum = 0;

	bp_recv = (void *)ip_recv + IP_UDP_HDR_SIZE;
	bp_recv->bp_op = OP_BOOTREPLY;
	bp_recv->bp_yiaddr = string_to_ip(DHCP_CLIENT_IP);
	bp_recv->bp_siaddr = string_to_ip(DHCP_SERVER_IP);
	copy_filename(bp_recv->bp_file, DHCP_FILE, sizeof(bp_recv->bp_file));

	memset(bp_recv->bp_vend, 0, sizeof(bp_recv->bp_vend));
	p = (uchar *)bp_recv->bp_vend;
	*(u32 *)p = htonl(DHCP_MAGIC);
	p += sizeof(u32);
	add_byte_opt(&p, OPT_MSG_TYPE,
		     type == DHCP_DISCOVER ? DHCP_OFFER : DHCP_ACK);
	add_ip_opt(&p, OPT_SERVERID, DHCP_SERVER_IP, NULL);
	add_ip_opt(&p, OPT_NETMASK, DHCP_NETMASK, NULL);
	add_ip_opt(&p, OPT_GATEWAY, DHCP_GATEWAY, NULL);
	add_ip_opt(&p, OPT_DNS, DHCP_DNS, DHCP_DNS2);
	add_str_opt(&p, OPT_HOSTNAME, DHCP_HOSTNAME);
	add_long_opt(&p, OPT_LEASE, DHCP_LEASE);
	add_str_opt(&p, OPT_BOOTFILE, DHCP_FILE);
	*p++ = OPT_END;

	/*
	 * The reply carries more options than the request, so its length has
	 * to be worked out rather than taken from the packet it was copied
	 * from
	 */
	optlen = p - (uchar *)bp_recv->bp_vend;
	iplen = IP_UDP_HDR_SIZE + BOOTP_HDR_SIZE - OPT_FIELD_SIZE + optlen;
	ip_recv->ip_len = htons(iplen);
	ip_recv->udp_len = htons(iplen - IP_HDR_SIZE);
	ip_recv->ip_sum = 0;
	ip_recv->ip_sum = compute_ip_checksum(ip_recv, IP_HDR_SIZE);

	priv->recv_packet_length[priv->recv_packets] = ETHER_HDR_SIZE + iplen;
	++priv->recv_packets;
	if (type == DHCP_DISCOVER)
		++ctx->offers;
	else
		++ctx->acks;

	return 0;
}

/**
 * tftp_req_to_reply() - serve the boot file to the client
 *
 * The options in the request are ignored, so the block size is the default of
 * 512 bytes and the whole file goes out in the first block. The ACK which comes
 * back ends the transfer and needs no answer.
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * @len: length of @packet in bytes
 * Return: 0 if a reply was queued, -EAGAIN if the packet is not a request
 */
static int tftp_req_to_reply(struct udevice *dev, void *packet,
			     unsigned int len)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct dhcp_ctx *ctx = priv->priv;
	struct ethernet_hdr *eth = packet, *eth_recv;
	struct ip_udp_hdr *ip = packet + ETHER_HDR_SIZE, *ip_recv;
	struct tftp_hdr *tftp, *tftp_recv;
	int size;

	if (ntohs(eth->et_protlen) != PROT_IP || ip->ip_p != IPPROTO_UDP)
		return -EAGAIN;

	tftp = (void *)ip + IP_UDP_HDR_SIZE;
	if (ntohs(ip->udp_dst) == TFTP_PORT) {
		if (ntohs(tftp->opcode) != TFTP_RRQ)
			return -EAGAIN;
		strlcpy(ctx->fname, (char *)tftp + sizeof(u16),
			sizeof(ctx->fname));
	} else if (ntohs(ip->udp_dst) == TFTP_TID) {
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

	size = DHCP_SIZE;
	eth_recv = (void *)priv->recv_packet_buffer[priv->recv_packets];
	memcpy(eth_recv->et_dest, eth->et_src, ARP_HLEN);
	memcpy(eth_recv->et_src, priv->fake_host_hwaddr, ARP_HLEN);
	eth_recv->et_protlen = htons(PROT_IP);

	ip_recv = (void *)eth_recv + ETHER_HDR_SIZE;
	ip_recv->ip_hl_v = 0x45;
	ip_recv->ip_tos = 0;
	ip_recv->ip_len = htons(IP_UDP_HDR_SIZE + TFTP_HDR_SIZE + size);
	ip_recv->ip_id = htons(1);
	ip_recv->ip_off = htons(IP_FLAGS_DFRAG);
	ip_recv->ip_ttl = 64;
	ip_recv->ip_p = IPPROTO_UDP;
	ip_recv->ip_sum = 0;
	ip_recv->ip_src = ip->ip_dst;
	ip_recv->ip_dst = ip->ip_src;
	ip_recv->ip_sum = compute_ip_checksum(ip_recv, IP_HDR_SIZE);
	ip_recv->udp_src = htons(TFTP_TID);
	ip_recv->udp_dst = ip->udp_src;
	ip_recv->udp_len = htons(UDP_HDR_SIZE + TFTP_HDR_SIZE + size);
	ip_recv->udp_xsum = 0;

	tftp_recv = (void *)ip_recv + IP_UDP_HDR_SIZE;
	tftp_recv->opcode = htons(TFTP_DATA);
	tftp_recv->block = htons(1);
	memcpy((void *)tftp_recv + TFTP_HDR_SIZE, DHCP_DATA, size);

	priv->recv_packet_length[priv->recv_packets] =
		ETHER_HDR_SIZE + IP_UDP_HDR_SIZE + TFTP_HDR_SIZE + size;
	++priv->recv_packets;

	return 0;
}

/**
 * sb_dhcp_handler() - act as a DHCP and TFTP server
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * @len: length of @packet in bytes
 * Return: 0, whether or not a reply was queued
 */
static int sb_dhcp_handler(struct udevice *dev, void *packet, unsigned int len)
{
	if (!sandbox_eth_arp_req_to_reply(dev, packet, len))
		return 0;
	if (!dhcp_req_to_reply(dev, packet, len))
		return 0;
	if (!tftp_req_to_reply(dev, packet, len))
		return 0;

	return 0;
}

/**
 * dhcp_setup() - point the dhcp command at the emulated server
 *
 * @uts: test state
 * @ctx: handler state, which must live until dhcp_restore() is called
 * Return: 0 if OK, 1 on failure
 */
static int dhcp_setup(struct unit_test_state *uts, struct dhcp_ctx *ctx)
{
	sandbox_eth_set_tx_handler(0, sb_dhcp_handler);
	sandbox_eth_set_priv(0, ctx);
	ut_assertok(env_set("ethact", DHCP_ETHACT));

	/*
	 * With CONFIG_BOOTP_SERVERIP the server address comes from the
	 * environment rather than from the reply. The setenv command is used
	 * rather than env_set(), since on_serverip() ignores a programmatic
	 * write and so net_server_ip would keep its old value.
	 */
	ut_assertok(run_command("setenv serverip " DHCP_SERVER_IP, 0));

	return 0;
}

/**
 * dhcp_restore() - put back the environment and the transmit handler
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int dhcp_restore(struct unit_test_state *uts)
{
	sandbox_eth_set_tx_handler(0, NULL);
	sandbox_eth_set_priv(0, NULL);
	ut_assertok(env_set("ethact", NULL));
	ut_assertok(run_command("setenv serverip", 0));
	ut_assertok(run_command("setenv ipaddr " DHCP_OWN_IP, 0));
	ut_assertok(env_set("netmask", NULL));
	ut_assertok(env_set("gatewayip", NULL));
	ut_assertok(env_set("dnsip", NULL));
	ut_assertok(env_set("dnsip2", NULL));
	ut_assertok(env_set("hostname", NULL));
	ut_assertok(env_set("bootfile", NULL));
	ut_assertok(env_set("autoload", NULL));
	ut_assertok(env_set("filesize", NULL));
	ut_assertok(env_set("fileaddr", NULL));

	return 0;
}

/**
 * check_bound() - check the line which reports the address obtained
 *
 * The message includes the time taken, which is not fixed.
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int check_bound(struct unit_test_state *uts)
{
	ut_assert_nextlinen("DHCP client bound to address " DHCP_CLIENT_IP
			    " (");

	return 0;
}

/**
 * check_params() - check the options of the reply which reach the environment
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int check_params(struct unit_test_state *uts)
{
	ut_asserteq_str(DHCP_CLIENT_IP, env_get("ipaddr"));
	ut_asserteq_str(DHCP_NETMASK, env_get("netmask"));
	ut_asserteq_str(DHCP_GATEWAY, env_get("gatewayip"));
	ut_asserteq_str(DHCP_DNS, env_get("dnsip"));
	if (IS_ENABLED(CONFIG_BOOTP_DNS2))
		ut_asserteq_str(DHCP_DNS2, env_get("dnsip2"));

	/*
	 * netboot_update_env() writes the host name only with CONFIG_CMD_BOOTP,
	 * which CMD_DHCP selects on the legacy stack this test is built for
	 */
	ut_asserteq_str(DHCP_HOSTNAME, env_get("hostname"));

	return 0;
}

/* Obtain a lease and load the boot file the server offers */
static int cmd_test_dhcp_base(struct unit_test_state *uts)
{
	struct dhcp_ctx ctx = { .uts = uts };

	ut_assertok(dhcp_setup(uts, &ctx));
	ut_assertok(run_commandf("dhcp %x", DHCP_LOAD_ADDR));
	ut_assert_nextline("BOOTP broadcast 1");
	ut_assertok(check_bound(uts));
	ut_assert_nextline("Using %s device", DHCP_ETHACT);
	ut_assert_nextline("TFTP from server " DHCP_SERVER_IP
			   "; our IP address is " DHCP_CLIENT_IP);
	ut_assert_nextline("Filename '%s'.", DHCP_FILE);
	ut_assert_nextline("Load address: 0x%x", DHCP_LOAD_ADDR);
	/*
	 * The transfer rate is reported only when the transfer takes a
	 * millisecond or more, which a file this small does not always manage
	 */
	ut_assert_nextlinen("Loading: ");
	ut_assert_skip_to_line("done");
	ut_assert_nextline("Bytes transferred = %d (%x hex)", DHCP_SIZE,
			   DHCP_SIZE);
	ut_assert_console_end();

	/* the lease takes an OFFER and an ACK, and the file one request */
	ut_asserteq(1, ctx.offers);
	ut_asserteq(1, ctx.acks);
	ut_asserteq_str(DHCP_FILE, ctx.fname);

	ut_asserteq(DHCP_SIZE, env_get_hex("filesize", 0));
	ut_asserteq_mem(DHCP_DATA, map_sysmem(DHCP_LOAD_ADDR, 0), DHCP_SIZE);
	ut_assertok(check_params(uts));

	ut_assertok(dhcp_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_dhcp_base, UTF_CONSOLE);

/* With autoload off the command obtains the lease and stops there */
static int cmd_test_dhcp_noload(struct unit_test_state *uts)
{
	struct dhcp_ctx ctx = { .uts = uts };

	ut_assertok(dhcp_setup(uts, &ctx));
	ut_assertok(env_set("autoload", "no"));
	ut_assertok(run_command("dhcp", 0));
	ut_assert_nextline("BOOTP broadcast 1");
	ut_assertok(check_bound(uts));
	ut_assert_console_end();

	ut_assertok(check_params(uts));
	ut_asserteq(1, ctx.offers);
	ut_asserteq(1, ctx.acks);

	/* Nothing was asked of the TFTP server */
	ut_asserteq_str("", ctx.fname);
	ut_assertok(dhcp_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_dhcp_noload, UTF_CONSOLE);

/* A file named on the command line is used in place of the offered one */
static int cmd_test_dhcp_file(struct unit_test_state *uts)
{
	struct dhcp_ctx ctx = { .uts = uts };

	ut_assertok(dhcp_setup(uts, &ctx));
	ut_assertok(run_commandf("dhcp %x other.img", DHCP_LOAD_ADDR));
	ut_assert_nextline("BOOTP broadcast 1");
	ut_assertok(check_bound(uts));
	ut_assert_skip_to_line("Filename 'other.img'.");
	ut_assert_nextline("Load address: 0x%x", DHCP_LOAD_ADDR);
	ut_assert_nextlinen("Loading: ");
	ut_assert_skip_to_line("done");
	ut_assert_nextline("Bytes transferred = %d (%x hex)", DHCP_SIZE,
			   DHCP_SIZE);
	ut_assert_console_end();

	ut_asserteq_str("other.img", ctx.fname);
	ut_assertok(dhcp_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_dhcp_file, UTF_CONSOLE);

/**
 * check_usage() - check the usage message the command shows
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("dhcp - boot image via network using DHCP/TFTP protocol");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("dhcp [loadAddress] [[hostIPaddr:]bootfilename]");
	ut_assert_console_end();

	return 0;
}

/* The command takes at most two arguments */
static int cmd_test_dhcp_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("dhcp 1000 boot.img extra", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_dhcp_usage, UTF_CONSOLE);
