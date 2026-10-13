// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the tftpsrv command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <dm.h>
#include <env.h>
#include <image.h>
#include <mapmem.h>
#include <net.h>
#include <asm/eth.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

/* Ethernet device used by the tests */
#define TFTPSRV_ETHACT	"eth@10002000"

/* Address sandbox starts with, which the command listens on */
#define TFTPSRV_OWN_IP	"192.0.2.1"

/* Address the emulated client writes from */
#define TFTPSRV_CLIENT_IP	"192.0.2.2"

/* Port the emulated client transfers from, chosen at random */
#define TFTPSRV_CLIENT_PORT	21314

/* Well-known TFTP port, which the command listens on */
#define TFTP_PORT	69

/* TFTP operations */
#define TFTP_WRQ	2
#define TFTP_DATA	3
#define TFTP_ACK	4

/* Address the tests receive the file at */
#define TFTPSRV_LOAD_ADDR	0x1000000

/* Name the emulated client writes, and the contents it sends */
#define TFTPSRV_FILE	"upload.img"
#define TFTPSRV_DATA	"U-Boot tftpsrv test file\n"
#define TFTPSRV_SIZE	((int)sizeof(TFTPSRV_DATA) - 1)

struct tftp_hdr {
	u16 opcode;
	u16 block;
};

#define TFTP_HDR_SIZE	sizeof(struct tftp_hdr)

/**
 * struct tftpsrv_ctx - state shared with the packet handlers
 *
 * @uts: test state, used by the ut_assert macros in the handlers
 * @requests: number of write requests sent to the command
 * @acks: number of ACKs the command has sent back
 * @blocks: number of data blocks sent to the command
 */
struct tftpsrv_ctx {
	struct unit_test_state *uts;
	int requests;
	int acks;
	int blocks;
};

/**
 * fill_ip_udp() - set up the IP and UDP headers of a packet to be received
 *
 * @eth_recv: Ethernet header of the packet being built
 * @src: port the emulated client sends from
 * @dst: port the packet is addressed to
 * @payload_len: number of payload bytes after the UDP header
 */
static void fill_ip_udp(struct ethernet_hdr *eth_recv, int src, int dst,
			int payload_len)
{
	struct ip_udp_hdr *ip_recv = (void *)eth_recv + ETHER_HDR_SIZE;

	ip_recv->ip_hl_v = 0x45;
	ip_recv->ip_tos = 0;
	ip_recv->ip_len = htons(IP_UDP_HDR_SIZE + payload_len);
	ip_recv->ip_id = htons(1);
	ip_recv->ip_off = htons(IP_FLAGS_DFRAG);
	ip_recv->ip_ttl = 64;
	ip_recv->ip_p = IPPROTO_UDP;
	ip_recv->ip_sum = 0;
	ip_recv->ip_src = string_to_ip(TFTPSRV_CLIENT_IP);
	ip_recv->ip_dst = net_ip;
	ip_recv->ip_sum = compute_ip_checksum(ip_recv, IP_HDR_SIZE);
	ip_recv->udp_src = htons(src);
	ip_recv->udp_dst = htons(dst);
	ip_recv->udp_len = htons(UDP_HDR_SIZE + payload_len);
	ip_recv->udp_xsum = 0;
}

/**
 * sb_tftpsrv_rx() - send the write request which starts the transfer
 *
 * The command transmits nothing while it waits, so the transmit handler cannot
 * get the exchange going. This runs on each idle poll instead and sends a
 * single write request naming the file and the 'octet' transfer mode.
 *
 * @dev: sandbox Ethernet device
 * Return: 0 if the request was queued, -EAGAIN if it has been sent already
 */
static int sb_tftpsrv_rx(struct udevice *dev)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct tftpsrv_ctx *ctx = priv->priv;
	struct ethernet_hdr *eth_recv;
	struct tftp_hdr *tftp_recv;
	int len;
	char *p;

	if (ctx->requests)
		return -EAGAIN;

	/* Don't allow the buffer to overrun */
	if (priv->recv_packets >= PKTBUFSRX)
		return -EAGAIN;

	eth_recv = (void *)priv->recv_packet_buffer[priv->recv_packets];
	memcpy(eth_recv->et_dest, pdata->enetaddr, ARP_HLEN);
	memcpy(eth_recv->et_src, priv->fake_host_hwaddr, ARP_HLEN);
	eth_recv->et_protlen = htons(PROT_IP);

	tftp_recv = (void *)eth_recv + ETHER_HDR_SIZE + IP_UDP_HDR_SIZE;
	tftp_recv->opcode = htons(TFTP_WRQ);

	/* the file name and the mode follow the opcode, each terminated */
	p = (char *)tftp_recv + sizeof(u16);
	memcpy(p, TFTPSRV_FILE, sizeof(TFTPSRV_FILE));
	p += sizeof(TFTPSRV_FILE);
	memcpy(p, "octet", sizeof("octet"));
	p += sizeof("octet");
	len = p - (char *)tftp_recv;

	fill_ip_udp(eth_recv, TFTPSRV_CLIENT_PORT, TFTP_PORT, len);

	priv->recv_packet_length[priv->recv_packets] =
		ETHER_HDR_SIZE + IP_UDP_HDR_SIZE + len;
	++priv->recv_packets;
	++ctx->requests;

	return 0;
}

/**
 * tftp_ack_to_data() - send the file in answer to the first ACK
 *
 * The command accepts the transfer with an ACK of block 0, from a port of its
 * own choosing, and the whole file goes back in the first block. The ACK of
 * that block ends the transfer and needs no answer.
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * @len: length of @packet in bytes
 * Return: 0 if a reply was queued, -EAGAIN if the packet is not for the client
 */
static int tftp_ack_to_data(struct udevice *dev, void *packet,
			    unsigned int len)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct tftpsrv_ctx *ctx = priv->priv;
	struct ethernet_hdr *eth = packet, *eth_recv;
	struct ip_udp_hdr *ip = packet + ETHER_HDR_SIZE;
	struct tftp_hdr *tftp, *tftp_recv;
	int size = TFTPSRV_SIZE;

	if (ntohs(eth->et_protlen) != PROT_IP || ip->ip_p != IPPROTO_UDP ||
	    ntohs(ip->udp_dst) != TFTPSRV_CLIENT_PORT)
		return -EAGAIN;

	tftp = (void *)ip + IP_UDP_HDR_SIZE;
	if (ntohs(tftp->opcode) != TFTP_ACK)
		return -EAGAIN;
	++ctx->acks;

	/* the transfer is over once the data block has been acknowledged */
	if (ntohs(tftp->block))
		return 0;

	/* Don't allow the buffer to overrun */
	if (priv->recv_packets >= PKTBUFSRX)
		return 0;

	eth_recv = (void *)priv->recv_packet_buffer[priv->recv_packets];
	memcpy(eth_recv->et_dest, eth->et_src, ARP_HLEN);
	memcpy(eth_recv->et_src, priv->fake_host_hwaddr, ARP_HLEN);
	eth_recv->et_protlen = htons(PROT_IP);

	tftp_recv = (void *)eth_recv + ETHER_HDR_SIZE + IP_UDP_HDR_SIZE;
	tftp_recv->opcode = htons(TFTP_DATA);
	tftp_recv->block = htons(1);
	memcpy((void *)tftp_recv + TFTP_HDR_SIZE, TFTPSRV_DATA, size);

	fill_ip_udp(eth_recv, TFTPSRV_CLIENT_PORT, ntohs(ip->udp_src),
		    TFTP_HDR_SIZE + size);

	priv->recv_packet_length[priv->recv_packets] =
		ETHER_HDR_SIZE + IP_UDP_HDR_SIZE + TFTP_HDR_SIZE + size;
	++priv->recv_packets;
	++ctx->blocks;

	return 0;
}

/**
 * sb_tftpsrv_tx() - act as a TFTP client writing a file
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * @len: length of @packet in bytes
 * Return: 0, whether or not a reply was queued
 */
static int sb_tftpsrv_tx(struct udevice *dev, void *packet, unsigned int len)
{
	if (!sandbox_eth_arp_req_to_reply(dev, packet, len))
		return 0;
	if (!tftp_ack_to_data(dev, packet, len))
		return 0;

	return 0;
}

/**
 * tftpsrv_setup() - point the command at the emulated client
 *
 * @uts: test state
 * @ctx: handler state, which must live until tftpsrv_restore() is called
 * @client: true to attach the handler which writes a file, false to leave the
 *	command listening with nothing to receive
 * Return: 0 if OK, 1 on failure
 */
static int tftpsrv_setup(struct unit_test_state *uts,
			 struct tftpsrv_ctx *ctx, bool client)
{
	sandbox_eth_set_tx_handler(0, client ? sb_tftpsrv_tx : NULL);
	sandbox_eth_set_rx_handler(0, client ? sb_tftpsrv_rx : NULL);
	sandbox_eth_set_priv(0, ctx);
	ut_assertok(env_set("ethact", TFTPSRV_ETHACT));

	/*
	 * The setenv command is used rather than env_set(), since on_ipaddr()
	 * ignores a programmatic write and so net_ip would keep its old value
	 */
	ut_assertok(run_command("setenv ipaddr " TFTPSRV_OWN_IP, 0));

	/*
	 * Drop any report of an earlier transfer, so that the tests can tell
	 * whether this command produces one. Under test.py the environment
	 * comes from a file which earlier runs have written to, so filesize is
	 * already set there
	 */
	ut_assertok(env_set("filesize", NULL));
	ut_assertok(env_set("fileaddr", NULL));

	return 0;
}

/**
 * tftpsrv_restore() - put back the environment and the packet handlers
 *
 * @uts: test state
 * @load_addr: value image_load_addr held before the command ran
 * Return: 0 if OK, 1 on failure
 */
static int tftpsrv_restore(struct unit_test_state *uts, ulong load_addr)
{
	sandbox_eth_set_tx_handler(0, NULL);
	sandbox_eth_set_rx_handler(0, NULL);
	sandbox_eth_set_priv(0, NULL);
	ut_assertok(env_set("ethact", NULL));
	ut_assertok(run_command("setenv ipaddr " TFTPSRV_OWN_IP, 0));
	ut_assertok(env_set("filesize", NULL));
	ut_assertok(env_set("fileaddr", NULL));
	image_load_addr = load_addr;

	return 0;
}

/**
 * check_transfer() - check the output the command produces for a transfer
 *
 * @uts: test state
 * @addr: address the file is expected to land at
 * Return: 0 if OK, 1 on failure
 */
static int check_transfer(struct unit_test_state *uts, ulong addr)
{
	ut_assert_nextline("Using %s device", TFTPSRV_ETHACT);
	ut_assert_nextline("Listening for TFTP transfer on " TFTPSRV_OWN_IP);
	ut_assert_nextline("Load address: 0x%lx", addr);

	/*
	 * The transfer rate is reported only when the transfer takes a
	 * millisecond or more, which a file this small does not always manage
	 */
	ut_assert_nextlinen("Loading: ");
	ut_assert_skip_to_line("done");
	ut_assert_nextline("Bytes transferred = %d (%x hex)", TFTPSRV_SIZE,
			   TFTPSRV_SIZE);
	ut_assert_console_end();

	return 0;
}

/* Receive a file from a client, at the address given on the command line */
static int cmd_test_tftpsrv_base(struct unit_test_state *uts)
{
	struct tftpsrv_ctx ctx = { .uts = uts };
	ulong load_addr = image_load_addr;

	ut_assertok(tftpsrv_setup(uts, &ctx, true));
	ut_assertok(run_commandf("tftpsrv %x", TFTPSRV_LOAD_ADDR));
	ut_assertok(check_transfer(uts, TFTPSRV_LOAD_ADDR));

	/* one request, one block, and an ACK of each */
	ut_asserteq(1, ctx.requests);
	ut_asserteq(1, ctx.blocks);
	ut_asserteq(2, ctx.acks);

	ut_asserteq(TFTPSRV_SIZE, env_get_hex("filesize", 0));
	ut_asserteq(TFTPSRV_LOAD_ADDR, env_get_hex("fileaddr", 0));
	ut_asserteq_mem(TFTPSRV_DATA, map_sysmem(TFTPSRV_LOAD_ADDR, 0),
			TFTPSRV_SIZE);

	ut_assertok(tftpsrv_restore(uts, load_addr));

	return 0;
}
CMD_TEST(cmd_test_tftpsrv_base, UTF_CONSOLE);

/* With no address given the loadaddr environment variable is used */
static int cmd_test_tftpsrv_loadaddr(struct unit_test_state *uts)
{
	struct tftpsrv_ctx ctx = { .uts = uts };
	ulong addr = TFTPSRV_LOAD_ADDR + 0x100;
	ulong load_addr = image_load_addr;
	char *prev;

	prev = strdup(env_get("loadaddr"));
	ut_assertnonnull(prev);
	ut_assertok(env_set_hex("loadaddr", addr));

	ut_assertok(tftpsrv_setup(uts, &ctx, true));
	ut_assertok(run_command("tftpsrv", 0));
	ut_assertok(check_transfer(uts, addr));

	ut_asserteq(addr, env_get_hex("fileaddr", 0));
	ut_asserteq_mem(TFTPSRV_DATA, map_sysmem(addr, 0), TFTPSRV_SIZE);

	ut_assertok(env_set("loadaddr", prev));
	free(prev);
	ut_assertok(tftpsrv_restore(uts, load_addr));

	return 0;
}
CMD_TEST(cmd_test_tftpsrv_loadaddr, UTF_CONSOLE);

/* Ctrl-C gives up on a transfer which never starts */
static int cmd_test_tftpsrv_abort(struct unit_test_state *uts)
{
	struct tftpsrv_ctx ctx = { .uts = uts };
	ulong load_addr = image_load_addr;
	int prev;

	/* leave the client out, so nothing ever arrives */
	ut_assertok(tftpsrv_setup(uts, &ctx, false));

	/*
	 * The command waits about 50 seconds for a transfer to start, so queue
	 * a Ctrl-C before running it. Sandbox turns off Ctrl-C checking, so
	 * enable it for the duration of the command
	 */
	console_in_puts("\x03");
	prev = disable_ctrlc(0);
	ut_asserteq(1, run_commandf("tftpsrv %x", TFTPSRV_LOAD_ADDR));
	disable_ctrlc(prev);

	ut_assert_nextline("Using %s device", TFTPSRV_ETHACT);
	ut_assert_nextline("Listening for TFTP transfer on " TFTPSRV_OWN_IP);
	ut_assert_nextline("Load address: 0x%x", TFTPSRV_LOAD_ADDR);
	ut_assert_nextlinen("Loading: ");
	ut_assert_nextline("Abort");
	ut_assert_console_end();

	ut_asserteq(0, ctx.requests);
	ut_assertnull(env_get("filesize"));

	ut_assertok(tftpsrv_restore(uts, load_addr));

	return 0;
}
CMD_TEST(cmd_test_tftpsrv_abort, UTF_CONSOLE);

/**
 * check_usage() - check the usage message the command prints
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("tftpsrv - act as a TFTP server and receive the first file");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("tftpsrv [loadAddress]");
	ut_assert_nextline("Listen for an incoming TFTP transfer and receive a file into memory.");
	ut_assert_nextline("The transfer is aborted if a transfer has not been started after");
	ut_assert_nextline("about 50 seconds or if Ctrl-C is pressed.");
	ut_assert_console_end();

	return 0;
}

/* The command takes at most one argument */
static int cmd_test_tftpsrv_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("tftpsrv 1000 extra", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_tftpsrv_usage, UTF_CONSOLE);

/* An option is refused rather than being taken as a file name */
static int cmd_test_tftpsrv_badopt(struct unit_test_state *uts)
{
	struct tftpsrv_ctx ctx = { .uts = uts };
	ulong load_addr = image_load_addr;

	/* the client is attached, so a command which listens would transfer */
	ut_assertok(tftpsrv_setup(uts, &ctx, true));
	ut_asserteq(1, run_command("tftpsrv -x", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(0, ctx.requests);
	ut_assertnull(env_get("filesize"));

	ut_assertok(tftpsrv_restore(uts, load_addr));

	return 0;
}
CMD_TEST(cmd_test_tftpsrv_badopt, UTF_CONSOLE);
