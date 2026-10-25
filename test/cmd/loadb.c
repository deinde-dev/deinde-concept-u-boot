// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the loadb, loadx and loady commands
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <env.h>
#include <errno.h>
#include <mapmem.h>
#include <vsprintf.h>
#include <asm/global_data.h>
#include <linux/string.h>
#include <u-boot/crc.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * The commands write to the address they are given, so the tests build one
 * from the region the board keeps for loading images; 0x1000 and 0x2000 past
 * it belong to the loads tests. A board with no default load address leaves
 * CONFIG_SYS_LOAD_ADDR at 0 and qemu-arm-sbsa has no memory below 4GB, so
 * each test checks the address against the start of memory first.
 */
#define LOADB_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x3000)

/* the file the tests download: printable, so kermit needs no escaping */
#define LOADB_DATA	"Hello, U-Boot!"
#define LOADB_SIZE	((int)sizeof(LOADB_DATA) - 1)

/* kermit adds this to a small number to make it printable */
#define KERM_SPACE	0x20

#define KERM_MARK	0x01	/* starts every packet */
#define KERM_EOL	0x0d	/* ends every packet */

/* the longest kermit stream a test sends: a data packet and a break packet */
#define KERM_MAX	(2 * LOADB_SIZE + 16)

#define XMODEM_DATA	128			/* data bytes in an SOH packet */
#define XMODEM_PKT	(XMODEM_DATA + 5)	/* whole packet, with the CRC */

#define XMODEM_SOH	0x01	/* starts a 128-byte packet */
#define XMODEM_EOT	0x04	/* ends the transmission */
#define XMODEM_EOF	0x1a	/* pads the last packet of a file */

/* three of these cancel a transmission */
#define XMODEM_CANCEL	"\x18\x18\x18"

/*
 * ymodem sends a header packet and a trailing one of its own, so its stream
 * is three packets and an end-of-transmission byte
 */
#define YMODEM_MAX	(3 * XMODEM_PKT + 1)

/*
 * loadb_can_store() - check that the address reaches memory the test can use
 */
static bool loadb_can_store(void)
{
	return gd->ram_base <= LOADB_ADDR;
}

/* kermit sends a small number as a printable character */
static char kerm_tochar(int val)
{
	return (char)((val + KERM_SPACE) & 0xff);
}

/**
 * kerm_packet() - build one kermit packet
 *
 * @buf: buffer to fill, which must hold @data plus six bytes
 * @seq: sequence number, which must differ from the previous packet, since
 *	the receiver reads a repeated one as a retry
 * @type: packet type: 'D' for data, 'B' to break off the transmission
 * @data: packet data, which must be printable and hold no '#'
 * Return: number of bytes written to @buf
 */
static int kerm_packet(char *buf, int seq, char type, const char *data)
{
	int len = strlen(data);
	int sum = 0;
	int i;

	buf[0] = KERM_MARK;
	buf[1] = kerm_tochar(len + 3);
	buf[2] = kerm_tochar(seq);
	buf[3] = type;
	memcpy(buf + 4, data, len);

	/* the check character covers the packet apart from the mark */
	for (i = 1; i < 4 + len; i++)
		sum += buf[i];
	buf[4 + len] = kerm_tochar((sum + ((sum >> 6) & 3)) & 0x3f);
	buf[5 + len] = KERM_EOL;

	return len + 6;
}

/**
 * xmodem_packet() - build one 128-byte xmodem or ymodem packet
 *
 * @buf: buffer to fill, which must hold XMODEM_PKT bytes
 * @blk: block number: 1 upwards for data, 0 for a ymodem header
 * @data: data for the start of the packet
 * @len: number of bytes of @data
 * @pad: byte to fill the rest of the packet with
 * Return: number of bytes written to @buf
 */
static int xmodem_packet(char *buf, int blk, const void *data, int len, int pad)
{
	u16 crc;

	buf[0] = XMODEM_SOH;
	buf[1] = blk;
	buf[2] = ~blk;
	memcpy(buf + 3, data, len);
	memset(buf + 3 + len, pad, XMODEM_DATA - len);

	crc = crc16_ccitt(0, (const u8 *)buf + 3, XMODEM_DATA);
	buf[3 + XMODEM_DATA] = crc >> 8;
	buf[4 + XMODEM_DATA] = crc & 0xff;

	return XMODEM_PKT;
}

/**
 * ymodem_header() - build the data for a ymodem header packet
 *
 * The packet holds the file name, a NUL, then the length in decimal.
 *
 * @buf: buffer to fill
 * @maxlen: size of @buf
 * @size: file length to report
 * Return: number of bytes written to @buf
 */
static int ymodem_header(char *buf, int maxlen, int size)
{
	int len;

	len = snprintf(buf, maxlen, "load.bin") + 1;

	return len + snprintf(buf + len, maxlen - len, "%d", size) + 1;
}

/* the line a command prints before it starts the transfer */
#define READY_FMT \
	"## Ready for binary (%s) download to 0x%08lX at %u bps..."

/**
 * check_ready() - check the line a command prints before the transfer
 *
 * @uts: test state
 * @mode: protocol the command names
 * Return: 0 if OK, other value on error
 */
static int check_ready(struct unit_test_state *uts, const char *mode)
{
	ut_assert_nextline(READY_FMT, mode, LOADB_ADDR, gd->baudrate);

	return 0;
}

/**
 * check_loaded() - check the end of a transfer which arrives
 *
 * A command answers each packet on the same console, and neither a kermit
 * reply nor an xmodem one ends in a newline, so the size line arrives with
 * those bytes in front of it. The size therefore comes from filesize rather
 * than from the console.
 *
 * @uts: test state
 * @ptr: where the file should have landed
 * Return: 0 if OK, other value on error
 */
static int check_loaded(struct unit_test_state *uts, const u8 *ptr)
{
	char size[16];

	ut_assert_skip_to_line("## Start Addr      = 0x%08lX", LOADB_ADDR);
	ut_assert_console_end();

	ut_asserteq_mem(LOADB_DATA, ptr, LOADB_SIZE);

	/* the command records the size in hex, with no leading zeros */
	snprintf(size, sizeof(size), "%x", LOADB_SIZE);
	ut_asserteq_str(size, env_get("filesize"));

	return 0;
}

/**
 * check_cancelled() - check the output of a cancelled xmodem transfer
 *
 * @uts: test state
 * @mode: protocol the command names
 * Return: 0 if OK, other value on error
 */
static int check_cancelled(struct unit_test_state *uts, const char *mode)
{
	/* the 'C' which asks for a CRC-mode transfer, then the cancellation */
	ut_assert_nextline("C");
	ut_assert_nextline("Cancelled");
	ut_assert_nextline("## Binary (%s) download aborted", mode);
	ut_assert_console_end();

	return 0;
}

/**
 * check_usage() - check that a command shows its usage message
 *
 * @uts: test state
 * @name: command name
 * @mode: protocol the command's help line names
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts, const char *name,
		       const char *mode)
{
	ut_assert_nextline("%s - load binary file over serial line (%s mode)",
			   name, mode);
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("%s [ addr [ baud ] ]", name);
	ut_assert_nextlinen("    - load binary file over serial line");
	ut_assert_console_end();

	return 0;
}

/* Test 'loadb' with a two-packet kermit transmission */
static int cmd_test_loadb_base(struct unit_test_state *uts)
{
	char stream[KERM_MAX];
	int len;
	u8 *ptr;

	if (!loadb_can_store())
		return -EAGAIN;

	ptr = map_sysmem(LOADB_ADDR, LOADB_SIZE);
	memset(ptr, '\0', LOADB_SIZE);

	len = kerm_packet(stream, 0, 'D', LOADB_DATA);
	len += kerm_packet(stream + len, 1, 'B', "");
	ut_asserteq(len, console_in_write(stream, len));

	ut_assertok(run_commandf("loadb %lx", LOADB_ADDR));
	ut_assertok(check_ready(uts, "kermit"));
	ut_assertok(check_loaded(uts, ptr));
	unmap_sysmem(ptr);

	return 0;
}
CMD_TEST(cmd_test_loadb_base, UTF_CONSOLE);

/* Test that ^C instead of a packet aborts a kermit transmission */
static int cmd_test_loadb_abort(struct unit_test_state *uts)
{
	if (!loadb_can_store())
		return -EAGAIN;

	ut_asserteq(1, console_in_write("\x03", 1));

	ut_asserteq(1, run_commandf("loadb %lx", LOADB_ADDR));
	ut_assertok(check_ready(uts, "kermit"));
	ut_assert_nextline("## Binary (kermit) download aborted");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_loadb_abort, UTF_CONSOLE);

/* Test that an option is refused rather than taken as an address */
static int cmd_test_loadb_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("loadb -x", 0));
	ut_assertok(check_usage(uts, "loadb", "kermit"));

	return 0;
}
CMD_TEST(cmd_test_loadb_usage, UTF_CONSOLE);

/* Test 'loadx' with a single xmodem packet padded out with ^Z */
static int cmd_test_loadx_base(struct unit_test_state *uts)
{
	char stream[XMODEM_PKT + 1];
	u8 *ptr;

	if (!loadb_can_store())
		return -EAGAIN;

	ptr = map_sysmem(LOADB_ADDR, LOADB_SIZE);
	memset(ptr, '\0', LOADB_SIZE);

	/*
	 * xmodem carries no length of its own, so the receiver takes the file
	 * as ending where the ^Z padding starts
	 */
	xmodem_packet(stream, 1, LOADB_DATA, LOADB_SIZE, XMODEM_EOF);
	stream[XMODEM_PKT] = XMODEM_EOT;
	ut_asserteq(XMODEM_PKT + 1, console_in_write(stream, XMODEM_PKT + 1));

	ut_assertok(run_commandf("loadx %lx", LOADB_ADDR));
	ut_assertok(check_ready(uts, "xmodem"));
	ut_assertok(check_loaded(uts, ptr));
	unmap_sysmem(ptr);

	return 0;
}
CMD_TEST(cmd_test_loadx_base, UTF_CONSOLE);

/* Test that three CAN characters cancel an xmodem transmission */
static int cmd_test_loadx_abort(struct unit_test_state *uts)
{
	if (!loadb_can_store())
		return -EAGAIN;

	ut_asserteq(3, console_in_write(XMODEM_CANCEL, 3));

	ut_asserteq(1, run_commandf("loadx %lx", LOADB_ADDR));
	ut_assertok(check_ready(uts, "xmodem"));
	ut_assertok(check_cancelled(uts, "xmodem"));

	return 0;
}
CMD_TEST(cmd_test_loadx_abort, UTF_CONSOLE);

/* Test that an option is refused rather than taken as an address */
static int cmd_test_loadx_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("loadx -x", 0));
	ut_assertok(check_usage(uts, "loadx", "xmodem"));

	return 0;
}
CMD_TEST(cmd_test_loadx_usage, UTF_CONSOLE);

/* Test 'loady' with a header packet, one data packet and a trailing packet */
static int cmd_test_loady_base(struct unit_test_state *uts)
{
	char stream[YMODEM_MAX];
	char hdr[32];
	int hlen, len;
	u8 *ptr;

	if (!loadb_can_store())
		return -EAGAIN;

	/*
	 * The whole stream has to be queued before the command starts, since
	 * nothing can add to the input buffer while the command reads it.
	 */
	if (CONFIG_CONSOLE_RECORD_IN_SIZE < YMODEM_MAX)
		return -EAGAIN;

	/* one byte more, to check that the padding does not reach memory */
	ptr = map_sysmem(LOADB_ADDR, LOADB_SIZE + 1);
	memset(ptr, '\0', LOADB_SIZE + 1);

	hlen = ymodem_header(hdr, sizeof(hdr), LOADB_SIZE);
	len = xmodem_packet(stream, 0, hdr, hlen, '\0');
	len += xmodem_packet(stream + len, 1, LOADB_DATA, LOADB_SIZE, '\0');
	stream[len++] = XMODEM_EOT;

	/* the transmission ends with a header packet holding no file name */
	len += xmodem_packet(stream + len, 0, "", 0, '\0');
	ut_asserteq(YMODEM_MAX, len);
	ut_asserteq(len, console_in_write(stream, len));

	ut_assertok(run_commandf("loady %lx", LOADB_ADDR));
	ut_assertok(check_ready(uts, "ymodem"));
	ut_assertok(check_loaded(uts, ptr));

	/*
	 * the data packet is padded out to 128 bytes, so the length in the
	 * header packet is what stops the padding being stored
	 */
	ut_asserteq(0, ptr[LOADB_SIZE]);
	unmap_sysmem(ptr);

	return 0;
}
CMD_TEST(cmd_test_loady_base, UTF_CONSOLE);

/* Test that three CAN characters cancel a ymodem transmission */
static int cmd_test_loady_abort(struct unit_test_state *uts)
{
	if (!loadb_can_store())
		return -EAGAIN;

	ut_asserteq(3, console_in_write(XMODEM_CANCEL, 3));

	ut_asserteq(1, run_commandf("loady %lx", LOADB_ADDR));
	ut_assertok(check_ready(uts, "ymodem"));
	ut_assertok(check_cancelled(uts, "ymodem"));

	return 0;
}
CMD_TEST(cmd_test_loady_abort, UTF_CONSOLE);

/* Test that an option is refused rather than taken as an address */
static int cmd_test_loady_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("loady -x", 0));
	ut_assertok(check_usage(uts, "loady", "ymodem"));

	return 0;
}
CMD_TEST(cmd_test_loady_usage, UTF_CONSOLE);
