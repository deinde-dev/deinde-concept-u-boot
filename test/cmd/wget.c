// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Linaro
 *
 * (C) Copyright 2022
 * Ying-Chun Liu (PaulLiu) <paul.liu@linaro.org>
 */

#include <command.h>
#include <dm.h>
#include <env.h>
#include <fdtdec.h>
#include <log.h>
#include <malloc.h>
#include <net.h>
#include <net/tcp.h>
#include <net/wget.h>
#include <asm/eth.h>
#include <dm/test.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

#define SHIFT_TO_TCPHDRLEN_FIELD(x) ((x) << 4)
#define LEN_B_TO_DW(x) ((x) >> 2)
#define GET_TCP_HDR_LEN_IN_BYTES(x) ((x) >> 2)

/* Ethernet device which the tests send from */
#define WGET_ETHACT	"eth@10002000"

/* Address of the emulated HTTP server */
#define WGET_SERVER_IP	"1.1.2.2"

/* Path which the emulated server provides */
#define WGET_PATH	"/index.html"

/* Address which the tests download to */
#define WGET_ADDR	0x20000

/* Address used to show that the command takes the one in $loadaddr */
#define WGET_LOADADDR	0x21000

/* Port which the command uses unless $httpdstp says otherwise */
#define WGET_PORT	80

/* Port which the port test asks for */
#define WGET_OTHER_PORT	8080

/* Number of bytes which the emulated server sends */
#define WGET_SIZE	29

/* md5 of those bytes, to show that they arrive intact */
#define WGET_MD5	"847d5e7320a27462e90bc1ed75eb8cd8"

/**
 * struct wget_ctx - state shared with the transmit handler
 *
 * @uts: test state, used by the ut_assert macros in the handler
 * @port: port which the last connection was made to
 * @requests: number of requests which the server has answered
 */
struct wget_ctx {
	struct unit_test_state *uts;
	int port;
	int requests;
};

static int sb_arp_handler(struct udevice *dev, void *packet,
			  unsigned int len)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct arp_hdr *arp = packet + ETHER_HDR_SIZE;
	int ret = 0;

	if (ntohs(arp->ar_op) == ARPOP_REQUEST) {
		priv->fake_host_ipaddr = net_read_ip(&arp->ar_spa);

		ret = sandbox_eth_recv_arp_req(dev);
		if (ret)
			return ret;
		ret = sandbox_eth_arp_req_to_reply(dev, packet, len);
		return ret;
	}

	return -EPROTONOSUPPORT;
}

static int sb_syn_handler(struct udevice *dev, void *packet,
			  unsigned int len)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct wget_ctx *ctx = priv->priv;
	struct ethernet_hdr *eth = packet;
	struct ip_tcp_hdr *tcp = packet + ETHER_HDR_SIZE;
	struct ethernet_hdr *eth_send;
	struct ip_tcp_hdr *tcp_send;

	/* Don't allow the buffer to overrun */
	if (priv->recv_packets >= PKTBUFSRX)
		return 0;

	ctx->port = ntohs(tcp->tcp_dst);

	eth_send = (void *)priv->recv_packet_buffer[priv->recv_packets];
	memcpy(eth_send->et_dest, eth->et_src, ARP_HLEN);
	memcpy(eth_send->et_src, priv->fake_host_hwaddr, ARP_HLEN);
	priv->irs = ntohl(tcp->tcp_seq);
	priv->iss = ~priv->irs; /* just to differ from irs */
	eth_send->et_protlen = htons(PROT_IP);
	tcp_send = (void *)eth_send + ETHER_HDR_SIZE;
	tcp_send->tcp_src = tcp->tcp_dst;
	tcp_send->tcp_dst = tcp->tcp_src;
	tcp_send->tcp_seq = htonl(priv->iss);
	tcp_send->tcp_ack = htonl(priv->irs + 1);
	tcp_send->tcp_hlen = SHIFT_TO_TCPHDRLEN_FIELD(LEN_B_TO_DW(TCP_HDR_SIZE));
	tcp_send->tcp_flags = TCP_SYN | TCP_ACK;
	tcp_send->tcp_win = htons(PKTBUFSRX * TCP_MSS >> TCP_SCALE);
	tcp_send->tcp_xsum = 0;
	tcp_send->tcp_ugr = 0;
	tcp_send->tcp_xsum = tcp_set_pseudo_header((uchar *)tcp_send,
						   tcp->ip_src,
						   tcp->ip_dst,
						   TCP_HDR_SIZE,
						   IP_TCP_HDR_SIZE);
	net_set_ip_header((uchar *)tcp_send,
			  tcp->ip_src,
			  tcp->ip_dst,
			  IP_TCP_HDR_SIZE,
			  IPPROTO_TCP);

	priv->recv_packet_length[priv->recv_packets] =
		ETHER_HDR_SIZE + IP_TCP_HDR_SIZE;
	++priv->recv_packets;

	return 0;
}

static int sb_ack_handler(struct udevice *dev, void *packet,
			  unsigned int len)
{
	struct eth_sandbox_priv *priv = dev_get_priv(dev);
	struct wget_ctx *ctx = priv->priv;
	struct ethernet_hdr *eth = packet;
	struct ip_tcp_hdr *tcp = packet + ETHER_HDR_SIZE;
	struct ethernet_hdr *eth_send;
	struct ip_tcp_hdr *tcp_send;
	void *data;
	int pkt_len;
	int payload_len = 0;
	u32 tcp_seq, tcp_ack;
	int tcp_data_len;
	const char *payload1 =
		/* response status line */
		"HTTP/1.1 200 OK\r\n"
		/* response header fields */
		"Date: Mon, 23 Dec 2024 05:18:23 GMT\r\n"
		"Server: Apache/2.4.62 (Debian)\r\n"
		"Last-Modified: Mon, 23 Dec 2024 05:04:50 GMT\r\n"
		"ETag: \"1d-629e8efb09e7b\"\r\n"
		"Accept-Ranges: bytes\r\n"
		"Content-Length: 29\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html\r\n"
		/* response header fields end marker */
		"\r\n"
		/* file data (for HTTP GET requests) */
		"<html><body>Hi</body></html>\n";

	/* Don't allow the buffer to overrun */
	if (priv->recv_packets >= PKTBUFSRX)
		return 0;

	eth_send = (void *)priv->recv_packet_buffer[priv->recv_packets];
	memcpy(eth_send->et_dest, eth->et_src, ARP_HLEN);
	memcpy(eth_send->et_src, priv->fake_host_hwaddr, ARP_HLEN);
	eth_send->et_protlen = htons(PROT_IP);
	tcp_send = (void *)eth_send + ETHER_HDR_SIZE;
	tcp_send->tcp_src = tcp->tcp_dst;
	tcp_send->tcp_dst = tcp->tcp_src;
	data = (void *)tcp_send + IP_TCP_HDR_SIZE;

	tcp_seq = ntohl(tcp->tcp_seq) - priv->irs;
	tcp_ack = ntohl(tcp->tcp_ack) - priv->iss;
	tcp_data_len = len - ETHER_HDR_SIZE - IP_HDR_SIZE - GET_TCP_HDR_LEN_IN_BYTES(tcp->tcp_hlen);

	if (tcp->tcp_flags & TCP_FIN)
		tcp_data_len++;

	tcp_send->tcp_seq = htonl(ntohl(tcp->tcp_ack));
	tcp_send->tcp_ack = htonl(ntohl(tcp->tcp_seq) + tcp_data_len);

	if (tcp_seq == 1 && tcp_ack == 1) {
		if (tcp_data_len == 0) {
			/* no data, wait for GET/HEAD request */
			return -1;
		}

		/* reply to GET/HEAD request */
		++ctx->requests;
		payload_len = strlen(payload1);
		memcpy(data, payload1, payload_len);
		tcp_send->tcp_flags = TCP_ACK;
	} else if (tcp_ack == 1 + strlen(payload1)) {
		payload_len = 0;
		tcp_send->tcp_flags = TCP_ACK | TCP_FIN;
	} else if (tcp_ack == 2 + strlen(payload1)) {
		payload_len = 0;
		tcp_send->tcp_flags = TCP_ACK;
	}

	tcp_send->tcp_hlen = SHIFT_TO_TCPHDRLEN_FIELD(LEN_B_TO_DW(TCP_HDR_SIZE));
	tcp_send->tcp_win = htons(PKTBUFSRX * TCP_MSS >> TCP_SCALE);
	tcp_send->tcp_xsum = 0;
	tcp_send->tcp_ugr = 0;
	pkt_len = IP_TCP_HDR_SIZE + payload_len;
	tcp_send->tcp_xsum = tcp_set_pseudo_header((uchar *)tcp_send,
						   tcp->ip_src,
						   tcp->ip_dst,
						   pkt_len - IP_HDR_SIZE,
						   pkt_len);
	net_set_ip_header((uchar *)tcp_send,
			  tcp->ip_src,
			  tcp->ip_dst,
			  pkt_len,
			  IPPROTO_TCP);

	priv->recv_packet_length[priv->recv_packets] =
		ETHER_HDR_SIZE + IP_TCP_HDR_SIZE + payload_len;
	++priv->recv_packets;

	return 0;
}

static int sb_http_handler(struct udevice *dev, void *packet,
			   unsigned int len)
{
	struct ethernet_hdr *eth = packet;
	struct ip_hdr *ip;
	struct ip_tcp_hdr *tcp;

	if (ntohs(eth->et_protlen) == PROT_ARP) {
		return sb_arp_handler(dev, packet, len);
	} else if (ntohs(eth->et_protlen) == PROT_IP) {
		ip = packet + ETHER_HDR_SIZE;
		if (ip->ip_p == IPPROTO_TCP) {
			tcp = packet + ETHER_HDR_SIZE;
			if (tcp->tcp_flags == TCP_SYN)
				return sb_syn_handler(dev, packet, len);
			else if (tcp->tcp_flags & TCP_ACK && !(tcp->tcp_flags & TCP_SYN))
				return sb_ack_handler(dev, packet, len);
			return 0;
		}
		return -EPROTONOSUPPORT;
	}

	return -EPROTONOSUPPORT;
}

/* Environment variables which the tests change and then put back */
static const char *const wget_vars[] = {
	"ethact", "ethrotate", "autostart", "serverip", "bootfile", "loadaddr",
	"httpdstp", "filesize", "fileaddr",
};

/**
 * struct wget_env - values which the variables in wget_vars[] hold on entry
 *
 * @val: value of each variable, or an empty string where it is not set
 */
struct wget_env {
	char val[ARRAY_SIZE(wget_vars)][64];
};

/**
 * wget_save_env() - note the environment variables which the tests change
 *
 * @env: returns the value of each variable
 */
static void wget_save_env(struct wget_env *env)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(wget_vars); i++) {
		const char *val = env_get(wget_vars[i]);

		strlcpy(env->val[i], val ? val : "", sizeof(env->val[i]));
	}
}

/**
 * wget_setup() - point the command at the emulated server
 *
 * @uts: test state
 * @ctx: handler state, which must live until wget_restore() is called
 * @server: true to attach the handler which answers a request, false to leave
 *	the command talking to nothing
 * Return: 0 if OK, 1 on failure
 */
static int wget_setup(struct unit_test_state *uts, struct wget_ctx *ctx,
		      bool server)
{
	sandbox_eth_set_tx_handler(0, server ? sb_http_handler : NULL);
	sandbox_eth_set_priv(0, ctx);

	ut_assertok(env_set("ethact", WGET_ETHACT));
	ut_assertok(env_set("ethrotate", "no"));

	/* a downloaded file must not be booted */
	ut_assertok(env_set("autostart", "no"));

	return 0;
}

/**
 * wget_restore() - put back the transmit handler and the environment
 *
 * @uts: test state
 * @env: values which wget_save_env() noted
 * Return: 0 if OK, 1 on failure
 */
static int wget_restore(struct unit_test_state *uts, struct wget_env *env)
{
	int i;

	sandbox_eth_set_tx_handler(0, NULL);
	sandbox_eth_set_priv(0, NULL);

	/*
	 * The setenv command is used rather than env_set(), since
	 * on_serverip() ignores a programmatic write, so net_server_ip would
	 * keep the address which the test set
	 */
	for (i = 0; i < ARRAY_SIZE(wget_vars); i++)
		ut_assertok(run_commandf("setenv %s %s", wget_vars[i],
					 env->val[i]));

	return 0;
}

/**
 * check_transfer() - check the output which a successful download produces
 *
 * @uts: test state
 * @addr: address which the file is expected to arrive at
 * Return: 0 if OK, 1 on failure
 */
static int check_transfer(struct unit_test_state *uts, ulong addr)
{
	/* the message starts with a newline, after the block markers */
	ut_assert_nextline_empty();
	ut_assert_nextline("Packets received 5, Transfer Successful");
	ut_assert_nextline("Bytes transferred = %d (%x hex)", WGET_SIZE,
			   WGET_SIZE);

	/* the bytes in memory are the ones which the server sent */
	ut_assertok(run_commandf("md5sum %lx %x", addr, WGET_SIZE));
	ut_assert_nextline("md5 for %08lx ... %08lx ==> " WGET_MD5, addr,
			   addr + WGET_SIZE - 1);
	ut_assert_console_end();

	/* the command reports where the file landed and how long it is */
	ut_asserteq(WGET_SIZE, env_get_hex("filesize", 0));
	ut_asserteq(addr, env_get_hex("fileaddr", 0));

	return 0;
}

/* Download a file from a server named on the command line */
static int cmd_test_wget_base(struct unit_test_state *uts)
{
	struct wget_ctx ctx = { .uts = uts };
	struct wget_env env;

	wget_save_env(&env);
	ut_assertok(wget_setup(uts, &ctx, true));

	ut_assertok(run_commandf("wget %x " WGET_SERVER_IP ":" WGET_PATH,
				 WGET_ADDR));
	ut_assertok(check_transfer(uts, WGET_ADDR));

	/* one request, to the well-known HTTP port */
	ut_asserteq(1, ctx.requests);
	ut_asserteq(WGET_PORT, ctx.port);

	ut_assertok(wget_restore(uts, &env));

	return 0;
}
CMD_TEST(cmd_test_wget_base, UTF_CONSOLE);

/*
 * With no arguments the address, the server and the path all come from the
 * environment
 */
static int cmd_test_wget_defaults(struct unit_test_state *uts)
{
	struct wget_ctx ctx = { .uts = uts };
	struct wget_env env;

	wget_save_env(&env);
	ut_assertok(wget_setup(uts, &ctx, true));

	ut_assertok(run_commandf("setenv serverip " WGET_SERVER_IP));
	ut_assertok(run_commandf("setenv loadaddr %x", WGET_LOADADDR));
	ut_assertok(env_set("bootfile", WGET_PATH));

	ut_assertok(run_command("wget", 0));
	ut_assertok(check_transfer(uts, WGET_LOADADDR));

	ut_asserteq(1, ctx.requests);

	ut_assertok(wget_restore(uts, &env));

	return 0;
}
CMD_TEST(cmd_test_wget_defaults, UTF_CONSOLE);

/* The httpdstp variable moves the transfer to another port */
static int cmd_test_wget_port(struct unit_test_state *uts)
{
	struct wget_ctx ctx = { .uts = uts };
	struct wget_env env;

	wget_save_env(&env);
	ut_assertok(wget_setup(uts, &ctx, true));

	ut_assertok(run_commandf("setenv httpdstp %d", WGET_OTHER_PORT));
	ut_assertok(run_commandf("wget %x " WGET_SERVER_IP ":" WGET_PATH,
				 WGET_ADDR));
	ut_assertok(check_transfer(uts, WGET_ADDR));

	ut_asserteq(WGET_OTHER_PORT, ctx.port);

	ut_assertok(wget_restore(uts, &env));

	return 0;
}
CMD_TEST(cmd_test_wget_port, UTF_CONSOLE);

/**
 * check_usage() - check the usage message which the command shows
 *
 * @uts: test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("wget - boot image via network using HTTP protocol");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("wget [loadAddress] [[hostIPaddr:]path and image name]");
	ut_assert_console_end();

	return 0;
}

/* The command takes at most two arguments */
static int cmd_test_wget_usage(struct unit_test_state *uts)
{
	struct wget_ctx ctx = { .uts = uts };
	struct wget_env env;

	wget_save_env(&env);

	/*
	 * The server is attached, so a command which asked for a file would
	 * get one
	 */
	ut_assertok(wget_setup(uts, &ctx, true));

	ut_asserteq(1, run_commandf("wget %x " WGET_SERVER_IP ":" WGET_PATH
				    " extra", WGET_ADDR));
	ut_assertok(check_usage(uts));
	ut_asserteq(0, ctx.requests);

	ut_assertok(wget_restore(uts, &env));

	return 0;
}
CMD_TEST(cmd_test_wget_usage, UTF_CONSOLE);

/* An option is refused rather than being taken as a path */
static int cmd_test_wget_badopt(struct unit_test_state *uts)
{
	struct wget_ctx ctx = { .uts = uts };
	struct wget_env env;

	wget_save_env(&env);

	/*
	 * The server is attached, so a command which asked for a file would
	 * get one
	 */
	ut_assertok(wget_setup(uts, &ctx, true));

	ut_asserteq(1, run_commandf("wget -x " WGET_SERVER_IP ":" WGET_PATH));
	ut_assertok(check_usage(uts));
	ut_asserteq(0, ctx.requests);

	ut_assertok(wget_restore(uts, &env));

	return 0;
}
CMD_TEST(cmd_test_wget_badopt, UTF_CONSOLE);

static int cmd_test_wget_uri_validate(struct unit_test_state *uts)
{
	ut_asserteq(true, wget_validate_uri("http://foo.com/bar.html"));
	ut_asserteq(true, wget_validate_uri("http://1.1.2.3/bar.html"));
	ut_asserteq(false, wget_validate_uri("http://foo/ba r.html"));
	ut_asserteq(false, wget_validate_uri("http://"));

	if (IS_ENABLED(CONFIG_WGET_HTTPS)) {
		ut_asserteq(true,
			    wget_validate_uri("https://foo.com/bar.html"));
		ut_asserteq(true,
			    wget_validate_uri("https://1.1.2.3/bar.html"));
		ut_asserteq(false, wget_validate_uri("https://foo/ba r.html"));
		ut_asserteq(false, wget_validate_uri("https://"));
	}

	return 0;
}
CMD_TEST(cmd_test_wget_uri_validate, UTF_CONSOLE);
