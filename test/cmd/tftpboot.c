// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the tftpboot command
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
#define TFTPBOOT_ETHACT		"eth@10002000"

/* Address sandbox starts with, which the command sends from */
#define TFTPBOOT_OWN_IP		"192.0.2.1"

/* Address of the emulated server */
#define TFTPBOOT_SERVER_IP	"192.0.2.2"

/* Another address the same emulated server answers on */
#define TFTPBOOT_OTHER_IP	"192.0.2.3"

/* Port the emulated server serves the file from */
#define TFTPBOOT_DATA_PORT	32770

/* Well-known TFTP port, which the read request goes to */
#define TFTP_PORT	69

/* TFTP operations */
#define TFTP_RRQ	1
#define TFTP_DATA	3
#define TFTP_ACK	4
#define TFTP_ERROR	5

/* TFTP error code for a file the server does not have */
#define TFTP_ERR_NO_FILE	1

/* Number of bytes in a TFTP block, before any option is negotiated */
#define TFTP_BLOCK	512

/* Address the tests load the file to */
#define TFTPBOOT_ADDR	0x1000000

/* Name of the file the tests ask for, and the contents they get back */
#define TFTPBOOT_FILE	"image.bin"
#define TFTPBOOT_DATA	"U-Boot tftpboot test file\n"
#define TFTPBOOT_SIZE	((int)sizeof(TFTPBOOT_DATA) - 1)

/* Name of a file the emulated server does not have */
#define TFTPBOOT_MISSING	"missing.bin"

/* Name the command makes up for itself, from the client address above */
#define TFTPBOOT_DEFAULT_FILE	"C0000201.img"

/* Size of a file long enough to need more than one TFTP block */
#define TFTPBOOT_BIG_SIZE	1200

/* Second line of the help message, which an IPv6 build adds */
#define TFTPBOOT_IPV6_HELP \
	"To use IPv6 add -ipv6 parameter or use IPv6 hostIPaddr framed with [] brackets"

struct tftp_hdr {
	u16 opcode;
	u16 block;
};

#define TFTP_HDR_SIZE	sizeof(struct tftp_hdr)

/**
 * struct tftpboot_ctx - state shared with the transmit handler
 *
 * @uts: test state, used by the ut_assert macros in the handler
 * @requests: number of read requests the server has seen
 * @blocks: number of data blocks the server has sent
 * @fname: name the last read request asked for
 * @data: bytes the server serves
 * @size: number of bytes in @data
 * @old_loadaddr: value of the loadaddr variable before the test, or NULL
 * @old_bootfile: value of the bootfile variable before the test, or NULL
 * @old_autostart: value of the autostart variable before the test, or NULL
 */
struct tftpboot_ctx {
	struct unit_test_state *uts;
	int requests;
	int blocks;
	char fname[64];
	const u8 *data;
	int size;
	char *old_loadaddr;
	char *old_bootfile;
	char *old_autostart;
};

/* Buffer holding the file the multi-block test asks for */
static u8 tftpboot_big[TFTPBOOT_BIG_SIZE];

/**
 * fill_ip_udp() - set up the IP and UDP headers of a packet to be received
 *
 * The reply comes from the address the request went to, so that a request sent
 * to either of the server's addresses is answered from that one.
 *
 * @eth_recv: Ethernet header of the packet being built
 * @ip: IP header of the packet U-Boot has just sent
 * @src: port the emulated server sends from
 * @payload_len: number of payload bytes after the UDP header
 */
static void fill_ip_udp(struct ethernet_hdr *eth_recv, struct ip_udp_hdr *ip,
			int src, int payload_len)
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
	ip_recv->ip_src = ip->ip_dst;
	ip_recv->ip_dst = ip->ip_src;
	ip_recv->ip_sum = compute_ip_checksum(ip_recv, IP_HDR_SIZE);
	ip_recv->udp_src = htons(src);
	ip_recv->udp_dst = ip->udp_src;
	ip_recv->udp_len = htons(UDP_HDR_SIZE + payload_len);
	ip_recv->udp_xsum = 0;
}

/**
 * send_block() - serve one block of the file
 *
 * The options in the request are ignored, so the block size is the default of
 * 512 bytes and the last block is the one which is shorter than that.
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * @block: block number to send, counting from 1
 * Return: 0 if the reply was queued, -EAGAIN if there was no room
 */
static int send_block(struct udevice *dev, void *packet, int block)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct tftpboot_ctx *ctx = priv->priv;
	struct ethernet_hdr *eth = packet, *eth_recv;
	struct ip_udp_hdr *ip = packet + ETHER_HDR_SIZE;
	struct tftp_hdr *tftp_recv;
	int offset = (block - 1) * TFTP_BLOCK;
	int size = min(ctx->size - offset, TFTP_BLOCK);

	/* Don't allow the buffer to overrun */
	if (priv->recv_packets >= PKTBUFSRX)
		return -EAGAIN;

	eth_recv = (void *)priv->recv_packet_buffer[priv->recv_packets];
	memcpy(eth_recv->et_dest, eth->et_src, ARP_HLEN);
	memcpy(eth_recv->et_src, priv->fake_host_hwaddr, ARP_HLEN);
	eth_recv->et_protlen = htons(PROT_IP);

	tftp_recv = (void *)eth_recv + ETHER_HDR_SIZE + IP_UDP_HDR_SIZE;
	tftp_recv->opcode = htons(TFTP_DATA);
	tftp_recv->block = htons(block);
	memcpy((void *)tftp_recv + TFTP_HDR_SIZE, ctx->data + offset, size);

	fill_ip_udp(eth_recv, ip, TFTPBOOT_DATA_PORT, TFTP_HDR_SIZE + size);

	priv->recv_packet_length[priv->recv_packets] =
		ETHER_HDR_SIZE + IP_UDP_HDR_SIZE + TFTP_HDR_SIZE + size;
	++priv->recv_packets;
	++ctx->blocks;

	return 0;
}

/**
 * send_error() - refuse a request for a file the server does not have
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * Return: 0 if the reply was queued, -EAGAIN if there was no room
 */
static int send_error(struct udevice *dev, void *packet)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct ethernet_hdr *eth = packet, *eth_recv;
	struct ip_udp_hdr *ip = packet + ETHER_HDR_SIZE;
	const char *msg = "File not found";
	struct tftp_hdr *tftp_recv;
	int len = strlen(msg) + 1;

	/* Don't allow the buffer to overrun */
	if (priv->recv_packets >= PKTBUFSRX)
		return -EAGAIN;

	eth_recv = (void *)priv->recv_packet_buffer[priv->recv_packets];
	memcpy(eth_recv->et_dest, eth->et_src, ARP_HLEN);
	memcpy(eth_recv->et_src, priv->fake_host_hwaddr, ARP_HLEN);
	eth_recv->et_protlen = htons(PROT_IP);

	/* the error code sits where the block number does in a data packet */
	tftp_recv = (void *)eth_recv + ETHER_HDR_SIZE + IP_UDP_HDR_SIZE;
	tftp_recv->opcode = htons(TFTP_ERROR);
	tftp_recv->block = htons(TFTP_ERR_NO_FILE);
	memcpy((void *)tftp_recv + TFTP_HDR_SIZE, msg, len);

	fill_ip_udp(eth_recv, ip, TFTPBOOT_DATA_PORT, TFTP_HDR_SIZE + len);

	priv->recv_packet_length[priv->recv_packets] =
		ETHER_HDR_SIZE + IP_UDP_HDR_SIZE + TFTP_HDR_SIZE + len;
	++priv->recv_packets;

	return 0;
}

/**
 * sb_tftpboot_tx() - act as a TFTP server serving a file
 *
 * The read request is answered with the first block, unless it names the file
 * the server does not have, in which case it gets an error. Each block is sent
 * when the one before it is acknowledged; the transfer ends with a block
 * shorter than 512 bytes, whose ACK needs no answer.
 *
 * @dev: sandbox Ethernet device
 * @packet: packet U-Boot has just sent
 * @len: length of @packet in bytes
 * Return: 0 if a reply was queued, -ve on error
 */
static int sb_tftpboot_tx(struct udevice *dev, void *packet, unsigned int len)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct tftpboot_ctx *ctx = priv->priv;
	struct ethernet_hdr *eth = packet;
	struct ip_udp_hdr *ip = packet + ETHER_HDR_SIZE;
	struct tftp_hdr *tftp;
	int block;

	if (ntohs(eth->et_protlen) == PROT_ARP)
		return sandbox_eth_arp_req_to_reply(dev, packet, len);

	if (ntohs(eth->et_protlen) != PROT_IP || ip->ip_p != IPPROTO_UDP)
		return -EPROTONOSUPPORT;

	tftp = (void *)ip + IP_UDP_HDR_SIZE;
	switch (ntohs(tftp->opcode)) {
	case TFTP_RRQ:
		if (ntohs(ip->udp_dst) != TFTP_PORT)
			return -EPROTONOSUPPORT;

		/* the file name follows the opcode, terminated */
		strlcpy(ctx->fname, (char *)tftp + sizeof(u16),
			sizeof(ctx->fname));
		++ctx->requests;

		if (!strcmp(ctx->fname, TFTPBOOT_MISSING))
			return send_error(dev, packet);

		return send_block(dev, packet, 1);

	case TFTP_ACK:
		if (ntohs(ip->udp_dst) != TFTPBOOT_DATA_PORT)
			return -EPROTONOSUPPORT;

		/* the transfer is over once the short block is acknowledged */
		block = ntohs(tftp->block);
		if (block * TFTP_BLOCK >= ctx->size)
			return 0;

		return send_block(dev, packet, block + 1);
	}

	return -EPROTONOSUPPORT;
}

/**
 * save_var() - keep the value of a variable the test is about to change
 *
 * @uts: test state
 * @name: name of the variable
 * @copyp: returns a copy of the value, which restore_var() frees, or NULL
 *	where the variable is unset
 * Return: 0 if OK, 1 on failure
 */
static int save_var(struct unit_test_state *uts, const char *name,
		    char **copyp)
{
	const char *val = env_get(name);

	*copyp = NULL;
	if (val) {
		*copyp = strdup(val);
		ut_assertnonnull(*copyp);
	}

	return 0;
}

/**
 * restore_var() - put a variable back as save_var() found it
 *
 * @uts: test state
 * @name: name of the variable
 * @copyp: copy taken by save_var(), freed here
 * Return: 0 if OK, 1 on failure
 */
static int restore_var(struct unit_test_state *uts, const char *name,
		       char **copyp)
{
	ut_assertok(env_set(name, *copyp));
	free(*copyp);
	*copyp = NULL;

	return 0;
}

/**
 * tftpboot_setup() - point the command at the emulated server
 *
 * @uts: test state
 * @ctx: handler state, which must live until tftpboot_restore() is called
 * Return: 0 if OK, 1 on failure
 */
static int tftpboot_setup(struct unit_test_state *uts,
			  struct tftpboot_ctx *ctx)
{
	ctx->data = (const u8 *)TFTPBOOT_DATA;
	ctx->size = TFTPBOOT_SIZE;

	sandbox_eth_set_tx_handler(0, sb_tftpboot_tx);
	sandbox_eth_set_priv(0, ctx);
	ut_assertok(env_set("ethact", TFTPBOOT_ETHACT));

	/*
	 * The setenv command is used rather than env_set(), since on_ipaddr()
	 * and on_serverip() ignore a programmatic write and so net_ip and
	 * net_server_ip would keep their old values
	 */
	ut_assertok(run_command("setenv ipaddr " TFTPBOOT_OWN_IP, 0));
	ut_assertok(run_command("setenv serverip " TFTPBOOT_SERVER_IP, 0));

	/*
	 * Under test.py the environment comes from a file which earlier runs
	 * have written to, so none of these can be assumed to be unset, and
	 * each must go back as it was: cmd_test_tftpsrv_loadaddr reads
	 * loadaddr and expects to find it
	 */
	ut_assertok(save_var(uts, "bootfile", &ctx->old_bootfile));
	ut_assertok(save_var(uts, "loadaddr", &ctx->old_loadaddr));
	ut_assertok(save_var(uts, "autostart", &ctx->old_autostart));
	ut_assertok(env_set("bootfile", NULL));
	ut_assertok(env_set("loadaddr", NULL));
	ut_assertok(env_set("autostart", NULL));

	return 0;
}

/**
 * tftpboot_restore() - put back the environment and the transmit handler
 *
 * The filesize and fileaddr variables are left as the command set them, as
 * they would be after a transfer on a real board.
 *
 * @uts: test state
 * @ctx: handler state
 * Return: 0 if OK, 1 on failure
 */
static int tftpboot_restore(struct unit_test_state *uts,
			    struct tftpboot_ctx *ctx)
{
	sandbox_eth_set_tx_handler(0, NULL);
	sandbox_eth_set_priv(0, NULL);
	ut_assertok(env_set("ethact", NULL));
	ut_assertok(run_command("setenv serverip", 0));
	ut_assertok(restore_var(uts, "bootfile", &ctx->old_bootfile));
	ut_assertok(restore_var(uts, "loadaddr", &ctx->old_loadaddr));
	ut_assertok(restore_var(uts, "autostart", &ctx->old_autostart));

	return 0;
}

/**
 * check_transfer() - check the output the command produces for a transfer
 *
 * @uts: test state
 * @server: address the file is expected to come from
 * @fname: name the file is expected to be read as
 * @addr: address the file is expected to be loaded to
 * @size: number of bytes the file is expected to hold
 * Return: 0 if OK, 1 on failure
 */
static int check_transfer(struct unit_test_state *uts, const char *server,
			  const char *fname, ulong addr, int size)
{
	ut_assert_nextline("Using %s device", TFTPBOOT_ETHACT);
	ut_assert_nextline("TFTP from server %s; our IP address is %s", server,
			   TFTPBOOT_OWN_IP);
	ut_assert_nextline("Filename '%s'.", fname);
	ut_assert_nextline("Load address: 0x%lx", addr);

	/*
	 * The transfer rate is reported only when the transfer takes a
	 * millisecond or more, which a file this small does not always manage
	 */
	ut_assert_nextlinen("Loading: ");
	ut_assert_skip_to_line("done");
	ut_assert_nextline("Bytes transferred = %d (%x hex)", size, size);
	ut_assert_console_end();

	/* the file is available for the commands which follow */
	ut_asserteq(size, env_get_hex("filesize", 0));
	ut_asserteq(addr, env_get_hex("fileaddr", 0));

	return 0;
}

/* Load a file from the server named by the serverip variable */
static int cmd_test_tftpboot_base(struct unit_test_state *uts)
{
	struct tftpboot_ctx ctx = { .uts = uts };

	ut_assertok(tftpboot_setup(uts, &ctx));
	ut_assertok(run_commandf("tftpboot %x %s", TFTPBOOT_ADDR,
				 TFTPBOOT_FILE));
	ut_assertok(check_transfer(uts, TFTPBOOT_SERVER_IP, TFTPBOOT_FILE,
				   TFTPBOOT_ADDR, TFTPBOOT_SIZE));

	/* one request and one block, which is all a file this size needs */
	ut_asserteq(1, ctx.requests);
	ut_asserteq(1, ctx.blocks);

	/* the server was asked for the name given, and its bytes arrived */
	ut_asserteq_str(TFTPBOOT_FILE, ctx.fname);
	ut_asserteq_mem(TFTPBOOT_DATA, map_sysmem(TFTPBOOT_ADDR, 0),
			TFTPBOOT_SIZE);

	ut_assertok(tftpboot_restore(uts, &ctx));

	return 0;
}
CMD_TEST(cmd_test_tftpboot_base, UTF_CONSOLE);

/* A file longer than one block arrives a block at a time, in order */
static int cmd_test_tftpboot_blocks(struct unit_test_state *uts)
{
	struct tftpboot_ctx ctx = { .uts = uts };
	int i;

	ut_assertok(tftpboot_setup(uts, &ctx));

	/* a pattern which shows up a block arriving out of order */
	for (i = 0; i < TFTPBOOT_BIG_SIZE; i++)
		tftpboot_big[i] = i * 7 + 3;
	ctx.data = tftpboot_big;
	ctx.size = TFTPBOOT_BIG_SIZE;

	ut_assertok(run_commandf("tftpboot %x %s", TFTPBOOT_ADDR,
				 TFTPBOOT_FILE));
	ut_assertok(check_transfer(uts, TFTPBOOT_SERVER_IP, TFTPBOOT_FILE,
				   TFTPBOOT_ADDR, TFTPBOOT_BIG_SIZE));

	/* three blocks: two full ones of 512 bytes and the remainder */
	ut_asserteq(1, ctx.requests);
	ut_asserteq(3, ctx.blocks);

	ut_asserteq_mem(tftpboot_big, map_sysmem(TFTPBOOT_ADDR, 0),
			TFTPBOOT_BIG_SIZE);

	ut_assertok(tftpboot_restore(uts, &ctx));

	return 0;
}
CMD_TEST(cmd_test_tftpboot_blocks, UTF_CONSOLE);

/* The bootfile and loadaddr variables provide what the command line omits */
static int cmd_test_tftpboot_env(struct unit_test_state *uts)
{
	struct tftpboot_ctx ctx = { .uts = uts };

	ut_assertok(tftpboot_setup(uts, &ctx));
	ut_assertok(env_set("bootfile", TFTPBOOT_FILE));
	ut_assertok(env_set_hex("loadaddr", TFTPBOOT_ADDR));
	ut_assertok(run_command("tftpboot", 0));
	ut_assertok(check_transfer(uts, TFTPBOOT_SERVER_IP, TFTPBOOT_FILE,
				   TFTPBOOT_ADDR, TFTPBOOT_SIZE));

	ut_asserteq_str(TFTPBOOT_FILE, ctx.fname);

	ut_assertok(tftpboot_restore(uts, &ctx));

	return 0;
}
CMD_TEST(cmd_test_tftpboot_env, UTF_CONSOLE);

/* A name on its own is taken as the file, leaving the address to loadaddr */
static int cmd_test_tftpboot_name(struct unit_test_state *uts)
{
	struct tftpboot_ctx ctx = { .uts = uts };

	ut_assertok(tftpboot_setup(uts, &ctx));
	ut_assertok(env_set_hex("loadaddr", TFTPBOOT_ADDR));
	ut_assertok(run_command("tftpboot " TFTPBOOT_FILE, 0));
	ut_assertok(check_transfer(uts, TFTPBOOT_SERVER_IP, TFTPBOOT_FILE,
				   TFTPBOOT_ADDR, TFTPBOOT_SIZE));

	ut_asserteq_str(TFTPBOOT_FILE, ctx.fname);

	ut_assertok(tftpboot_restore(uts, &ctx));

	return 0;
}
CMD_TEST(cmd_test_tftpboot_name, UTF_CONSOLE);

/* With no name anywhere the command makes one up from the client address */
static int cmd_test_tftpboot_noname(struct unit_test_state *uts)
{
	struct tftpboot_ctx ctx = { .uts = uts };

	ut_assertok(tftpboot_setup(uts, &ctx));
	ut_assertok(run_commandf("tftpboot %x", TFTPBOOT_ADDR));
	ut_assert_nextline("*** Warning: no boot file name; using '%s'",
			   TFTPBOOT_DEFAULT_FILE);
	ut_assertok(check_transfer(uts, TFTPBOOT_SERVER_IP,
				   TFTPBOOT_DEFAULT_FILE, TFTPBOOT_ADDR,
				   TFTPBOOT_SIZE));

	ut_asserteq_str(TFTPBOOT_DEFAULT_FILE, ctx.fname);

	ut_assertok(tftpboot_restore(uts, &ctx));

	return 0;
}
CMD_TEST(cmd_test_tftpboot_noname, UTF_CONSOLE);

/* An address before the file name overrides the server for that transfer */
static int cmd_test_tftpboot_server(struct unit_test_state *uts)
{
	struct tftpboot_ctx ctx = { .uts = uts };

	ut_assertok(tftpboot_setup(uts, &ctx));
	ut_assertok(run_commandf("tftpboot %x " TFTPBOOT_OTHER_IP ":%s",
				 TFTPBOOT_ADDR, TFTPBOOT_FILE));
	ut_assertok(check_transfer(uts, TFTPBOOT_OTHER_IP, TFTPBOOT_FILE,
				   TFTPBOOT_ADDR, TFTPBOOT_SIZE));

	/* the serverip variable is left as it was */
	ut_asserteq_str(TFTPBOOT_SERVER_IP, env_get("serverip"));
	ut_asserteq_str(TFTPBOOT_FILE, ctx.fname);

	ut_assertok(tftpboot_restore(uts, &ctx));

	return 0;
}
CMD_TEST(cmd_test_tftpboot_server, UTF_CONSOLE);

/* A file the server does not have ends the transfer with its error */
static int cmd_test_tftpboot_missing(struct unit_test_state *uts)
{
	struct tftpboot_ctx ctx = { .uts = uts };

	ut_assertok(tftpboot_setup(uts, &ctx));
	ut_asserteq(1, run_commandf("tftpboot %x %s", TFTPBOOT_ADDR,
				    TFTPBOOT_MISSING));
	ut_assert_nextline("Using %s device", TFTPBOOT_ETHACT);
	ut_assert_nextline("TFTP from server " TFTPBOOT_SERVER_IP
			   "; our IP address is " TFTPBOOT_OWN_IP);
	ut_assert_nextline("Filename '%s'.", TFTPBOOT_MISSING);
	ut_assert_nextline("Load address: 0x%x", TFTPBOOT_ADDR);
	ut_assert_nextlinen("Loading: ");
	ut_assert_nextline("TFTP error: 'File not found' (%d)",
			   TFTP_ERR_NO_FILE);
	ut_assert_nextline("Not retrying...");
	ut_assert_console_end();

	/* the request was refused, so no block arrived */
	ut_asserteq(1, ctx.requests);
	ut_asserteq(0, ctx.blocks);

	ut_assertok(tftpboot_restore(uts, &ctx));

	return 0;
}
CMD_TEST(cmd_test_tftpboot_missing, UTF_CONSOLE);

/**
 * check_usage() - check the usage message the command shows
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	if (IS_ENABLED(CONFIG_IPV6)) {
		ut_assert_nextline("tftpboot - boot image via network using TFTP protocol");
		ut_assert_nextline(TFTPBOOT_IPV6_HELP);
		ut_assert_nextline_empty();
		ut_assert_nextline("Usage:");
		ut_assert_nextline("tftpboot [loadAddress] [[hostIPaddr:]bootfilename] [-ipv6]");
	} else {
		ut_assert_nextline("tftpboot - load file via network using TFTP protocol");
		ut_assert_nextline_empty();
		ut_assert_nextline("Usage:");
		ut_assert_nextline("tftpboot [loadAddress] [[hostIPaddr:]bootfilename]");
	}
	ut_assert_console_end();

	return 0;
}

/* The command takes at most two arguments, or three with IPv6 */
static int cmd_test_tftpboot_usage(struct unit_test_state *uts)
{
	struct tftpboot_ctx ctx = { .uts = uts };

	/* the server is attached, so a command which ran would load a file */
	ut_assertok(tftpboot_setup(uts, &ctx));

	ut_asserteq(1, run_commandf("tftpboot %x %s extra more", TFTPBOOT_ADDR,
				    TFTPBOOT_FILE));
	ut_assertok(check_usage(uts));

	ut_asserteq(0, ctx.requests);

	ut_assertok(tftpboot_restore(uts, &ctx));

	return 0;
}
CMD_TEST(cmd_test_tftpboot_usage, UTF_CONSOLE);
