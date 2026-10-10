// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the pcap command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <env.h>
#include <malloc.h>
#include <mapmem.h>
#include <vsprintf.h>
#include <linux/string.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

/* Ethernet device and host the tests use */
#define PCAP_ETHACT	"eth@10002000"
#define PCAP_HOST	"1.1.2.2"

/* Size of the buffer the tests capture into */
#define BUF_SIZE	4096

/* A buffer with room for the file header and one packet, but no more */
#define SMALL_SIZE	100

/* Magic number, snapshot length and link type of a PCAP file */
#define PCAP_MAGIC	0xa1b2c3d4
#define PCAP_SNAPLEN	65535
#define PCAP_ETHERNET	1

/* Sizes of the file header and of the header before each packet */
#define FILE_HDR_SIZE	24
#define REC_HDR_SIZE	16

/*
 * Length of every packet of a ping on sandbox: an Ethernet header followed by
 * either an ARP packet or an IP header with an empty ICMP echo
 */
#define PING_PKT_LEN	42

/* Packets a ping exchanges in each direction, and the file size that comes to */
#define PING_PKTS	2
#define PING_SIZE	(FILE_HDR_SIZE + 2 * PING_PKTS * \
			 (REC_HDR_SIZE + PING_PKT_LEN))

/* Last line of the help text, which ends the usage message */
#define USAGE_LAST	"\t<max_size>: Maximum size of pcap file (decimal)"

/* Warning given when the buffer cannot hold another packet */
#define FULL_LINE \
	"!!! Buffer is full, consider increasing buffer size !!!"

/* Byte written over the file header to show that a refused init writes nothing */
#define SPOIL_BYTE	0x55

/**
 * struct pcap_hdr - file header, from struct pcap_header in net/pcap.c
 *
 * @magic: magic number, which also tells a reader the byte order
 * @version_major: major version of the file format
 * @version_minor: minor version of the file format
 * @thiszone: offset of the timestamps from UTC, which U-Boot leaves at zero
 * @sigfigs: accuracy of the timestamps, which U-Boot leaves at zero
 * @snaplen: longest packet which may be recorded
 * @network: link type of the packets, 1 for Ethernet
 */
struct pcap_hdr {
	u32 magic;
	u16 version_major;
	u16 version_minor;
	s32 thiszone;
	u32 sigfigs;
	u32 snaplen;
	u32 network;
};

/**
 * struct pcap_rec - header which precedes each packet
 *
 * @ts_sec: seconds part of the timestamp
 * @ts_usec: microseconds part of the timestamp
 * @incl_len: number of bytes of the packet which are stored
 * @orig_len: length of the packet on the wire
 */
struct pcap_rec {
	u32 ts_sec;
	u32 ts_usec;
	u32 incl_len;
	u32 orig_len;
};

/**
 * struct pcap_ctx - buffer a test captures into
 *
 * @buf: buffer, as the test sees it
 * @addr: address of @buf, as the command wants it
 * @size: size of @buf in bytes
 */
struct pcap_ctx {
	void *buf;
	ulong addr;
	uint size;
};

/**
 * pcap_setup() - obtain a buffer and point the capture at it
 *
 * The buffer comes from malloc() rather than from a fixed address, so that the
 * test writes only over memory of its own.
 *
 * @uts: test state
 * @ctx: returns the buffer in use
 * @size: size of buffer to ask for
 * Return: 0 if OK, 1 on failure
 */
static int pcap_setup(struct unit_test_state *uts, struct pcap_ctx *ctx,
		      uint size)
{
	ctx->buf = malloc(size);
	ut_assertnonnull(ctx->buf);
	ctx->addr = map_to_sysmem(ctx->buf);
	ctx->size = size;

	ut_assertok(run_commandf("pcap init %lx %u", ctx->addr, size));
	ut_assert_nextline("PCAP capture initialized: addr: 0x%lx max length: %u",
			   ctx->addr, size);
	ut_assert_console_end();

	ut_assertok(env_set("ethact", PCAP_ETHACT));

	return 0;
}

/**
 * pcap_restore() - stop the capture and give the buffer back
 *
 * There is no way to undo 'pcap init', so the capture is stopped instead. That
 * is enough to keep a later network command from recording into memory which is
 * no longer ours.
 *
 * @uts: test state
 * @ctx: buffer in use
 * Return: 0 if OK, 1 on failure
 */
static int pcap_restore(struct unit_test_state *uts, struct pcap_ctx *ctx)
{
	ut_assertok(run_command("pcap stop", 0));
	ut_assertok(env_set("ethact", NULL));
	free(ctx->buf);
	ctx->buf = NULL;
	ut_assert_console_end();

	return 0;
}

/**
 * check_file_hdr() - check the PCAP file header in the buffer
 *
 * @uts: test state
 * @ctx: buffer holding the capture
 * Return: 0 if OK, 1 on failure
 */
static int check_file_hdr(struct unit_test_state *uts, struct pcap_ctx *ctx)
{
	const struct pcap_hdr *hdr = ctx->buf;

	ut_asserteq(PCAP_MAGIC, hdr->magic);
	ut_asserteq(2, hdr->version_major);
	ut_asserteq(4, hdr->version_minor);
	ut_asserteq(0, hdr->thiszone);
	ut_asserteq(0, hdr->sigfigs);
	ut_asserteq(PCAP_SNAPLEN, hdr->snaplen);
	ut_asserteq(PCAP_ETHERNET, hdr->network);
	ut_asserteq(FILE_HDR_SIZE, sizeof(*hdr));

	return 0;
}

/**
 * check_status_lines() - check the four lines of a status report
 *
 * @uts: test state
 * @ctx: buffer holding the capture
 * @active: true if the capture is expected to be recording
 * @used: expected number of bytes used
 * @in: expected number of incoming packets
 * @out: expected number of outgoing packets
 * Return: 0 if OK, 1 on failure
 */
static int check_status_lines(struct unit_test_state *uts,
			      struct pcap_ctx *ctx, bool active, uint used,
			      uint in, uint out)
{
	ut_assert_nextline("PCAP status:");
	ut_assert_nextline("\tInitialized addr: 0x%lx\tmax length: %u",
			   ctx->addr, ctx->size);
	ut_assert_nextline("\tStatus: %s.\t file size: %u",
			   active ? "Active" : "Idle", used);
	ut_assert_nextline("\tIncoming packets: %u Outgoing packets: %u", in,
			   out);

	return 0;
}

/**
 * check_status() - run 'pcap status' and check what it reports
 *
 * @uts: test state
 * @ctx: buffer holding the capture
 * @active: true if the capture is expected to be recording
 * @used: expected number of bytes used
 * @in: expected number of incoming packets
 * @out: expected number of outgoing packets
 * Return: 0 if OK, 1 on failure
 */
static int check_status(struct unit_test_state *uts, struct pcap_ctx *ctx,
			bool active, uint used, uint in, uint out)
{
	ut_assertok(run_command("pcap status", 0));
	ut_assertok(check_status_lines(uts, ctx, active, used, in, out));
	ut_assert_console_end();

	return 0;
}

/**
 * check_records() - walk the records in the buffer
 *
 * Every packet of a ping on sandbox is the same length, so each record header
 * is checked against that and the walk must land exactly on @used.
 *
 * @uts: test state
 * @ctx: buffer holding the capture
 * @used: number of bytes the capture reports using
 * @countp: returns the number of records found
 * Return: 0 if OK, 1 on failure
 */
static int check_records(struct unit_test_state *uts, struct pcap_ctx *ctx,
			 uint used, uint *countp)
{
	uint pos = FILE_HDR_SIZE;
	uint count = 0;

	while (pos < used) {
		const struct pcap_rec *rec = ctx->buf + pos;

		ut_asserteq(PING_PKT_LEN, rec->incl_len);
		ut_asserteq(PING_PKT_LEN, rec->orig_len);
		pos += REC_HDR_SIZE + rec->incl_len;
		count++;
	}
	ut_asserteq(used, pos);
	*countp = count;

	return 0;
}

/* Capture a ping and read the file back */
static int cmd_test_pcap_base(struct unit_test_state *uts)
{
	struct pcap_ctx ctx;
	char size[8];
	uint count;

	/*
	 * Every sub-command but init needs a buffer. This has to come before
	 * any test sets one up, since nothing can undo 'pcap init'; the test
	 * is first in the file so that it runs first.
	 */
	ut_asserteq(1, run_command("pcap status", 0));
	ut_assert_nextline("pcap was not initialized");
	ut_asserteq(1, run_command("pcap start", 0));
	ut_assert_nextline("error: pcap was not initialized");
	ut_asserteq(1, run_command("pcap stop", 0));
	ut_assert_nextline("error: pcap was not initialized");
	ut_asserteq(1, run_command("pcap clear", 0));
	ut_assert_nextline("error: pcap was not initialized");
	ut_assert_console_end();

	ut_assertok(pcap_setup(uts, &ctx, BUF_SIZE));

	/* the file header is written at once, but nothing is recorded yet */
	ut_assertok(check_file_hdr(uts, &ctx));
	ut_assertok(check_status(uts, &ctx, false, FILE_HDR_SIZE, 0, 0));

	/*
	 * The network stack reports the capture when a command finishes, so the
	 * ping prints the status of its own accord
	 */
	ut_assertok(run_command("pcap start", 0));
	ut_assertok(run_commandf("ping %s", PCAP_HOST));
	ut_assert_nextline("Using %s device", PCAP_ETHACT);
	ut_assertok(check_status_lines(uts, &ctx, true, PING_SIZE, PING_PKTS,
				       PING_PKTS));
	ut_assert_nextline("host %s is alive", PCAP_HOST);
	ut_assert_console_end();

	/* an ARP exchange and an echo exchange, each of two packets */
	ut_assertok(check_records(uts, &ctx, PING_SIZE, &count));
	ut_asserteq(2 * PING_PKTS, count);

	/* the length of the file reaches the environment in hexadecimal */
	sprintf(size, "%x", PING_SIZE);
	ut_asserteq_str(size, env_get("pcapsize"));

	/* nothing more is recorded, or reported, once the capture is stopped */
	ut_assertok(run_command("pcap stop", 0));
	ut_assertok(run_commandf("ping %s", PCAP_HOST));
	ut_assert_nextline("Using %s device", PCAP_ETHACT);
	ut_assert_nextline("host %s is alive", PCAP_HOST);
	ut_assert_console_end();
	ut_assertok(check_status(uts, &ctx, false, PING_SIZE, PING_PKTS,
				 PING_PKTS));

	ut_assertok(pcap_restore(uts, &ctx));

	return 0;
}
CMD_TEST(cmd_test_pcap_base, UTF_CONSOLE);

/* Clearing a capture keeps the buffer and starts again */
static int cmd_test_pcap_clear(struct unit_test_state *uts)
{
	struct pcap_ctx ctx;
	uint count;

	ut_assertok(pcap_setup(uts, &ctx, BUF_SIZE));
	ut_assertok(run_command("pcap start", 0));
	ut_assertok(run_commandf("ping %s", PCAP_HOST));
	console_record_reset();

	/* the counts and the file size go back to where they started */
	ut_assertok(run_command("pcap clear", 0));
	ut_assert_nextline("pcap capture cleared");
	ut_assert_console_end();
	ut_assertok(check_status(uts, &ctx, true, FILE_HDR_SIZE, 0, 0));
	ut_assertok(check_file_hdr(uts, &ctx));

	/* so the next exchange fills the same buffer again */
	ut_assertok(run_commandf("ping %s", PCAP_HOST));
	console_record_reset();
	ut_assertok(check_status(uts, &ctx, true, PING_SIZE, PING_PKTS,
				 PING_PKTS));
	ut_assertok(check_records(uts, &ctx, PING_SIZE, &count));
	ut_asserteq(2 * PING_PKTS, count);

	ut_assertok(pcap_restore(uts, &ctx));

	return 0;
}
CMD_TEST(cmd_test_pcap_clear, UTF_CONSOLE);

/* A buffer too small for the traffic fills up and says so */
static int cmd_test_pcap_full(struct unit_test_state *uts)
{
	uint used = FILE_HDR_SIZE + REC_HDR_SIZE + PING_PKT_LEN;
	struct pcap_ctx ctx;
	uint count;

	ut_assertok(pcap_setup(uts, &ctx, SMALL_SIZE));
	ut_assertok(run_command("pcap start", 0));

	/* the warning is printed where the packet would have gone */
	ut_assertok(run_commandf("ping %s", PCAP_HOST));
	ut_assert_nextline("Using %s device", PCAP_ETHACT);
	ut_assert_nextline_empty();
	ut_assert_nextline(FULL_LINE);
	ut_assertok(check_status_lines(uts, &ctx, true, used, 0, 1));
	ut_assert_nextline("host %s is alive", PCAP_HOST);
	ut_assert_console_end();

	/* only the first packet of the exchange is in the buffer */
	ut_assertok(check_records(uts, &ctx, used, &count));
	ut_asserteq(1, count);

	/* clearing it lets packets through again, until it fills once more */
	ut_assertok(run_command("pcap clear", 0));
	ut_assert_nextline("pcap capture cleared");
	ut_assert_console_end();
	ut_assertok(run_commandf("ping %s", PCAP_HOST));
	ut_assert_nextline("Using %s device", PCAP_ETHACT);
	ut_assert_nextline_empty();
	ut_assert_nextline(FULL_LINE);
	ut_assertok(check_status_lines(uts, &ctx, true, used, 0, 1));
	ut_assert_nextline("host %s is alive", PCAP_HOST);
	ut_assert_console_end();

	ut_assertok(pcap_restore(uts, &ctx));

	return 0;
}
CMD_TEST(cmd_test_pcap_full, UTF_CONSOLE);

/* A buffer with no room for the file header is refused */
static int cmd_test_pcap_small(struct unit_test_state *uts)
{
	struct pcap_ctx ctx;
	u8 *buf;
	int i;

	ut_assertok(pcap_setup(uts, &ctx, BUF_SIZE));

	/* spoil the header, so that any write by the refused init shows up */
	buf = ctx.buf;
	memset(buf, SPOIL_BYTE, FILE_HDR_SIZE);

	ut_asserteq(1, run_commandf("pcap init %lx %u", ctx.addr,
				    FILE_HDR_SIZE - 1));
	ut_assert_nextline("PCAP buffer must hold at least %u bytes",
			   FILE_HDR_SIZE);
	ut_assert_console_end();

	for (i = 0; i < FILE_HDR_SIZE; i++)
		ut_asserteq(SPOIL_BYTE, buf[i]);

	/* and the capture is left pointing where it was */
	ut_assertok(check_status(uts, &ctx, false, FILE_HDR_SIZE, 0, 0));

	/* room for the header alone is enough to set a buffer up */
	ut_assertok(run_commandf("pcap init %lx %u", ctx.addr,
				 FILE_HDR_SIZE));
	ut_assert_nextline("PCAP capture initialized: addr: 0x%lx max length: %u",
			   ctx.addr, FILE_HDR_SIZE);
	ut_assert_console_end();
	ut_assertok(check_file_hdr(uts, &ctx));

	ctx.size = FILE_HDR_SIZE;
	ut_assertok(check_status(uts, &ctx, false, FILE_HDR_SIZE, 0, 0));

	ut_assertok(pcap_restore(uts, &ctx));

	return 0;
}
CMD_TEST(cmd_test_pcap_small, UTF_CONSOLE);

/* Check the usage message, which every bad command line produces */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("pcap - pcap");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("pcap - network packet capture");
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_nextline_empty();
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}

/* Test 'pcap' with a bad command line */
static int cmd_test_pcap_usage(struct unit_test_state *uts)
{
	/* a sub-command is needed, and must be one the command knows */
	ut_asserteq(1, run_command("pcap", 0));
	ut_assertok(check_usage(uts));
	ut_asserteq(1, run_command("pcap bananas", 0));
	ut_assertok(check_usage(uts));

	/* init takes an address and a size, no fewer and no more */
	ut_asserteq(1, run_command("pcap init", 0));
	ut_assertok(check_usage(uts));
	ut_asserteq(1, run_command("pcap init 1000", 0));
	ut_assertok(check_usage(uts));
	ut_asserteq(1, run_command("pcap init 1000 100 8", 0));
	ut_assertok(check_usage(uts));

	/* the others take nothing at all */
	ut_asserteq(1, run_command("pcap start now", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_pcap_usage, UTF_CONSOLE);
