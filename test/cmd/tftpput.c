// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the tftpput command
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

/* Ethernet device used by the tests */
#define TFTPPUT_ETHACT	"eth@10002000"

/* Address sandbox starts with, which the command sends from */
#define TFTPPUT_OWN_IP	"192.0.2.1"

/* Address of the emulated server */
#define TFTPPUT_SERVER_IP	"192.0.2.2"

/* Port the emulated server answers the write request from */
#define TFTPPUT_DATA_PORT	32769

/* Well-known TFTP port, which the write request goes to */
#define TFTP_PORT	69

/* TFTP operations */
#define TFTP_WRQ	2
#define TFTP_DATA	3
#define TFTP_ACK	4

/* Address the tests send the file from */
#define TFTPPUT_ADDR	0x1000000

/* Name the tests write, and the contents they send */
#define TFTPPUT_FILE	"upload.img"
#define TFTPPUT_DATA	"U-Boot tftpput test file\n"
#define TFTPPUT_SIZE	((int)sizeof(TFTPPUT_DATA) - 1)

/* Name the command makes up for itself, from the address above */
#define TFTPPUT_DEFAULT_FILE	"C0000201.img"

/* Size of a file long enough to need more than one TFTP block */
#define TFTPPUT_BIG_SIZE	1200

struct tftp_hdr {
	u16 opcode;
	u16 block;
};

#define TFTP_HDR_SIZE	sizeof(struct tftp_hdr)

/**
 * struct tftpput_ctx - state shared with the transmit handler
 *
 * @uts: test state, used by the ut_assert macros in the handler
 * @requests: number of write requests the server has accepted
 * @blocks: number of data blocks the server has acknowledged
 * @fname: name the last write request asked for
 * @data: bytes the server has been sent
 * @len: number of bytes in @data
 */
struct tftpput_ctx {
	struct unit_test_state *uts;
	int requests;
	int blocks;
	char fname[64];
	char data[TFTPPUT_BIG_SIZE];
	int len;
};

/**
 * fill_ip_udp() - set up the IP and UDP headers of a packet to be received
 *
 * @eth_recv: Ethernet header of the packet being built
 * @src: port the emulated server sends from
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
	ip_recv->ip_src = string_to_ip(TFTPPUT_SERVER_IP);
	ip_recv->ip_dst = net_ip;
	ip_recv->ip_sum = compute_ip_checksum(ip_recv, IP_HDR_SIZE);
	ip_recv->udp_src = htons(src);
	ip_recv->udp_dst = htons(dst);
	ip_recv->udp_len = htons(UDP_HDR_SIZE + payload_len);
	ip_recv->udp_xsum = 0;
}

/**
 * send_ack() - acknowledge a block, as a TFTP server does
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * @block: block number to acknowledge, 0 to accept the transfer
 * Return: 0 if the reply was queued, -EAGAIN if there was no room
 */
static int send_ack(struct udevice *dev, void *packet, int block)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct ethernet_hdr *eth = packet, *eth_recv;
	struct ip_udp_hdr *ip = packet + ETHER_HDR_SIZE;
	struct tftp_hdr *tftp_recv;

	/* Don't allow the buffer to overrun */
	if (priv->recv_packets >= PKTBUFSRX)
		return -EAGAIN;

	eth_recv = (void *)priv->recv_packet_buffer[priv->recv_packets];
	memcpy(eth_recv->et_dest, eth->et_src, ARP_HLEN);
	memcpy(eth_recv->et_src, priv->fake_host_hwaddr, ARP_HLEN);
	eth_recv->et_protlen = htons(PROT_IP);

	tftp_recv = (void *)eth_recv + ETHER_HDR_SIZE + IP_UDP_HDR_SIZE;
	tftp_recv->opcode = htons(TFTP_ACK);
	tftp_recv->block = htons(block);

	fill_ip_udp(eth_recv, TFTPPUT_DATA_PORT, ntohs(ip->udp_src),
		    TFTP_HDR_SIZE);

	priv->recv_packet_length[priv->recv_packets] =
		ETHER_HDR_SIZE + IP_UDP_HDR_SIZE + TFTP_HDR_SIZE;
	++priv->recv_packets;

	return 0;
}

/**
 * sb_tftpput_tx() - act as a TFTP server receiving a file
 *
 * The write request is accepted with an ACK of block 0, which also tells the
 * command which port to send the data to. Each data block is kept and
 * acknowledged; the command ends the transfer once a short block is
 * acknowledged.
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * @len: length of @packet in bytes
 * Return: 0 if a reply was queued, -ve on error
 */
static int sb_tftpput_tx(struct udevice *dev, void *packet, unsigned int len)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct tftpput_ctx *ctx = priv->priv;
	struct ethernet_hdr *eth = packet;
	struct ip_udp_hdr *ip = packet + ETHER_HDR_SIZE;
	struct tftp_hdr *tftp;
	int dlen;

	if (ntohs(eth->et_protlen) == PROT_ARP)
		return sandbox_eth_arp_req_to_reply(dev, packet, len);

	if (ntohs(eth->et_protlen) != PROT_IP || ip->ip_p != IPPROTO_UDP)
		return -EPROTONOSUPPORT;

	tftp = (void *)ip + IP_UDP_HDR_SIZE;
	switch (ntohs(tftp->opcode)) {
	case TFTP_WRQ:
		if (ntohs(ip->udp_dst) != TFTP_PORT)
			return -EPROTONOSUPPORT;

		/* the file name follows the opcode, terminated */
		strlcpy(ctx->fname, (char *)tftp + sizeof(u16),
			sizeof(ctx->fname));
		++ctx->requests;

		return send_ack(dev, packet, 0);

	case TFTP_DATA:
		if (ntohs(ip->udp_dst) != TFTPPUT_DATA_PORT)
			return -EPROTONOSUPPORT;

		dlen = ntohs(ip->udp_len) - UDP_HDR_SIZE - TFTP_HDR_SIZE;
		if (ctx->len + dlen <= sizeof(ctx->data)) {
			memcpy(ctx->data + ctx->len,
			       (void *)tftp + TFTP_HDR_SIZE, dlen);
			ctx->len += dlen;
		}
		++ctx->blocks;

		return send_ack(dev, packet, ntohs(tftp->block));
	}

	return -EPROTONOSUPPORT;
}

/**
 * tftpput_setup() - point the command at the emulated server
 *
 * @uts: test state
 * @ctx: handler state, which must live until tftpput_restore() is called
 * @server: true to attach the handler which accepts the file, false to leave
 *	the command talking to nothing
 * Return: 0 if OK, 1 on failure
 */
static int tftpput_setup(struct unit_test_state *uts,
			 struct tftpput_ctx *ctx, bool server)
{
	sandbox_eth_set_tx_handler(0, server ? sb_tftpput_tx : NULL);
	sandbox_eth_set_priv(0, ctx);
	ut_assertok(env_set("ethact", TFTPPUT_ETHACT));

	/*
	 * The setenv command is used rather than env_set(), since on_ipaddr()
	 * and on_serverip() ignore a programmatic write and so net_ip and
	 * net_server_ip would keep their old values
	 */
	ut_assertok(run_command("setenv ipaddr " TFTPPUT_OWN_IP, 0));
	ut_assertok(run_command("setenv serverip " TFTPPUT_SERVER_IP, 0));

	/*
	 * Under test.py the environment comes from a file which earlier runs
	 * have written to, so neither variable can be assumed to be unset
	 */
	ut_assertok(env_set("bootfile", NULL));
	ut_assertok(env_set("autostart", NULL));

	/* put the file where the command is told to read it from */
	memcpy(map_sysmem(TFTPPUT_ADDR, TFTPPUT_SIZE), TFTPPUT_DATA,
	       TFTPPUT_SIZE);

	return 0;
}

/**
 * tftpput_restore() - put back the environment and the transmit handler
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int tftpput_restore(struct unit_test_state *uts)
{
	sandbox_eth_set_tx_handler(0, NULL);
	sandbox_eth_set_priv(0, NULL);
	ut_assertok(env_set("ethact", NULL));
	ut_assertok(run_command("setenv serverip", 0));
	ut_assertok(env_set("bootfile", NULL));

	return 0;
}

/**
 * check_transfer() - check the output the command produces for a transfer
 *
 * @uts: test state
 * @fname: name the file is expected to be written as
 * @size: number of bytes the file is expected to hold
 * Return: 0 if OK, 1 on failure
 */
static int check_transfer(struct unit_test_state *uts, const char *fname,
			  int size)
{
	ut_assert_nextline("Using %s device", TFTPPUT_ETHACT);
	ut_assert_nextline("TFTP to server " TFTPPUT_SERVER_IP
			   "; our IP address is " TFTPPUT_OWN_IP);
	ut_assert_nextline("Filename '%s'.", fname);
	ut_assert_nextline("Save address: 0x%x", TFTPPUT_ADDR);
	ut_assert_nextline("Save size:    0x%x", size);

	/*
	 * The transfer rate is reported only when the transfer takes a
	 * millisecond or more, which a file this small does not always manage
	 */
	ut_assert_nextlinen("Saving: ");
	ut_assert_skip_to_line("done");
	ut_assert_nextline("Bytes transferred = %d (%x hex)", size, size);
	ut_assert_console_end();

	return 0;
}

/* Send a file to a server named on the command line */
static int cmd_test_tftpput_base(struct unit_test_state *uts)
{
	struct tftpput_ctx ctx = { .uts = uts };

	ut_assertok(tftpput_setup(uts, &ctx, true));
	ut_assertok(run_commandf("tftpput %x %x " TFTPPUT_SERVER_IP ":%s",
				 TFTPPUT_ADDR, TFTPPUT_SIZE, TFTPPUT_FILE));
	ut_assertok(check_transfer(uts, TFTPPUT_FILE, TFTPPUT_SIZE));

	/* one request and one block, which is all a file this size needs */
	ut_asserteq(1, ctx.requests);
	ut_asserteq(1, ctx.blocks);

	/* the server got the name it was given and the bytes in memory */
	ut_asserteq_str(TFTPPUT_FILE, ctx.fname);
	ut_asserteq(TFTPPUT_SIZE, ctx.len);
	ut_asserteq_mem(TFTPPUT_DATA, ctx.data, TFTPPUT_SIZE);

	ut_assertok(tftpput_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_tftpput_base, UTF_CONSOLE);

/* With no file name the command makes one up from its own address */
static int cmd_test_tftpput_noname(struct unit_test_state *uts)
{
	struct tftpput_ctx ctx = { .uts = uts };

	ut_assertok(tftpput_setup(uts, &ctx, true));
	ut_assertok(run_commandf("tftpput %x %x", TFTPPUT_ADDR, TFTPPUT_SIZE));
	ut_assert_nextline("*** Warning: no boot file name; using '%s'",
			   TFTPPUT_DEFAULT_FILE);
	ut_assertok(check_transfer(uts, TFTPPUT_DEFAULT_FILE, TFTPPUT_SIZE));

	ut_asserteq_str(TFTPPUT_DEFAULT_FILE, ctx.fname);
	ut_asserteq(TFTPPUT_SIZE, ctx.len);

	ut_assertok(tftpput_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_tftpput_noname, UTF_CONSOLE);

/* The bootfile variable provides the name when the command line does not */
static int cmd_test_tftpput_bootfile(struct unit_test_state *uts)
{
	struct tftpput_ctx ctx = { .uts = uts };

	ut_assertok(tftpput_setup(uts, &ctx, true));
	ut_assertok(env_set("bootfile", TFTPPUT_FILE));
	ut_assertok(run_commandf("tftpput %x %x", TFTPPUT_ADDR, TFTPPUT_SIZE));
	ut_assertok(check_transfer(uts, TFTPPUT_FILE, TFTPPUT_SIZE));

	ut_asserteq_str(TFTPPUT_FILE, ctx.fname);

	ut_assertok(tftpput_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_tftpput_bootfile, UTF_CONSOLE);

/* A file longer than one block is sent a block at a time, in order */
static int cmd_test_tftpput_blocks(struct unit_test_state *uts)
{
	struct tftpput_ctx ctx = { .uts = uts };
	u8 *buf;
	int i;

	ut_assertok(tftpput_setup(uts, &ctx, true));

	/* a pattern which shows up a block arriving out of order */
	buf = map_sysmem(TFTPPUT_ADDR, TFTPPUT_BIG_SIZE);
	for (i = 0; i < TFTPPUT_BIG_SIZE; i++)
		buf[i] = i * 7 + 3;

	ut_assertok(run_commandf("tftpput %x %x " TFTPPUT_SERVER_IP ":%s",
				 TFTPPUT_ADDR, TFTPPUT_BIG_SIZE,
				 TFTPPUT_FILE));
	ut_assertok(check_transfer(uts, TFTPPUT_FILE, TFTPPUT_BIG_SIZE));

	/* three blocks: two full ones of 512 bytes and the remainder */
	ut_asserteq(1, ctx.requests);
	ut_asserteq(3, ctx.blocks);

	ut_asserteq(TFTPPUT_BIG_SIZE, ctx.len);
	ut_asserteq_mem(buf, ctx.data, TFTPPUT_BIG_SIZE);

	ut_assertok(tftpput_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_tftpput_blocks, UTF_CONSOLE);

/**
 * check_usage() - check the usage message the command shows
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("tftpput - TFTP put command, for uploading files to a server");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("tftpput Address Size [[hostIPaddr:]filename]");
	ut_assert_console_end();

	return 0;
}

/* Both the address and the size are required */
static int cmd_test_tftpput_usage(struct unit_test_state *uts)
{
	struct tftpput_ctx ctx = { .uts = uts };

	/* the server is attached, so a command which sent a file would work */
	ut_assertok(tftpput_setup(uts, &ctx, true));

	/* neither no arguments nor the address alone is enough */
	ut_asserteq(1, run_command("tftpput", 0));
	ut_assertok(check_usage(uts));
	ut_asserteq(1, run_commandf("tftpput %x", TFTPPUT_ADDR));
	ut_assertok(check_usage(uts));

	/* and the command takes at most three */
	ut_asserteq(1, run_commandf("tftpput %x %x %s extra", TFTPPUT_ADDR,
				    TFTPPUT_SIZE, TFTPPUT_FILE));
	ut_assertok(check_usage(uts));

	ut_asserteq(0, ctx.requests);

	ut_assertok(tftpput_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_tftpput_usage, UTF_CONSOLE);

/* The address and the size must both be hex numbers */
static int cmd_test_tftpput_badnum(struct unit_test_state *uts)
{
	struct tftpput_ctx ctx = { .uts = uts };

	ut_assertok(tftpput_setup(uts, &ctx, true));

	ut_asserteq(1, run_commandf("tftpput wrong %x", TFTPPUT_SIZE));
	ut_assert_nextline("Invalid address/size");
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_commandf("tftpput %x wrong", TFTPPUT_ADDR));
	ut_assert_nextline("Invalid address/size");
	ut_assertok(check_usage(uts));

	ut_asserteq(0, ctx.requests);

	ut_assertok(tftpput_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_tftpput_badnum, UTF_CONSOLE);

/* An option is refused rather than being taken as an address */
static int cmd_test_tftpput_badopt(struct unit_test_state *uts)
{
	struct tftpput_ctx ctx = { .uts = uts };

	/* the server is attached, so a command which sent a file would work */
	ut_assertok(tftpput_setup(uts, &ctx, true));

	ut_asserteq(1, run_commandf("tftpput -x %x", TFTPPUT_SIZE));
	ut_assertok(check_usage(uts));
	ut_asserteq(0, ctx.requests);

	ut_assertok(tftpput_restore(uts));

	return 0;
}
CMD_TEST(cmd_test_tftpput_badopt, UTF_CONSOLE);
