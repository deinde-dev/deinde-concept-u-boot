// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the md command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <compiler.h>
#include <console.h>
#include <errno.h>
#include <mapmem.h>
#include <asm/global_data.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Memory these tests display. A board with no default load address leaves
 * CONFIG_SYS_LOAD_ADDR at 0 and qemu-arm-sbsa has no memory below 4GB, so each
 * test checks this against the start of memory.
 */
#define MD_ADDR		((ulong)CONFIG_SYS_LOAD_ADDR + 0x6000)
#define MD_SIZE		0x200

/*
 * Value the region is filled with. None of its bytes is printable and all of
 * them are the same, so the dump reads alike on a big-endian board such as
 * qemu_malta and on a little-endian one
 */
#define MD_VALUE	0xdddddddd

/* Text md shows for a full line of the pattern, at each size */
#define LINE_B		"dd dd dd dd dd dd dd dd dd dd dd dd dd dd dd dd"
#define LINE_W		"dddd dddd dddd dddd dddd dddd dddd dddd"
#define LINE_L		"dddddddd dddddddd dddddddd dddddddd"
#define LINE_Q		"dddddddddddddddd dddddddddddddddd"

/* ASCII column for such a line, which has no printable byte in it */
#define LINE_ASCII	"................"

/* Fill the region with a known value, so md has something to show */
static void setup_mem(void)
{
	u32 *buf;
	int i;

	buf = map_sysmem(MD_ADDR, MD_SIZE);
	for (i = 0; i < MD_SIZE / sizeof(u32); i++)
		buf[i] = MD_VALUE;
	unmap_sysmem(buf);
}

/*
 * check_line() - Check one line of the dump
 *
 * @uts: Test state
 * @addr: Address the line starts at
 * @data: Text md shows for the data on that line
 * Return: 0 if OK, other value on error
 */
static int check_line(struct unit_test_state *uts, ulong addr,
		      const char *data)
{
	ut_assert_nextline("%08lx: %s  %s", addr, data, LINE_ASCII);

	return 0;
}

/* Test 'md' showing a line of memory */
static int cmd_test_md_base(struct unit_test_state *uts)
{
	if (gd->ram_base > MD_ADDR)
		return -EAGAIN;
	setup_mem();

	/* the default size is a long, so four values fill a 16-byte line */
	ut_assertok(run_commandf("md %lx 4", MD_ADDR));
	ut_assertok(check_line(uts, MD_ADDR, LINE_L));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_md_base, UTF_CONSOLE);

/* Test each data size, with enough values to fill a line */
static int cmd_test_md_size(struct unit_test_state *uts)
{
	if (gd->ram_base > MD_ADDR)
		return -EAGAIN;
	setup_mem();

	ut_assertok(run_commandf("md.b %lx 10", MD_ADDR));
	ut_assertok(check_line(uts, MD_ADDR, LINE_B));

	ut_assertok(run_commandf("md.w %lx 8", MD_ADDR));
	ut_assertok(check_line(uts, MD_ADDR, LINE_W));

	ut_assertok(run_commandf("md.l %lx 4", MD_ADDR));
	ut_assertok(check_line(uts, MD_ADDR, LINE_L));

	if (MEM_SUPPORT_64BIT_DATA) {
		ut_assertok(run_commandf("md.q %lx 2", MD_ADDR));
		ut_assertok(check_line(uts, MD_ADDR, LINE_Q));
	}
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_md_size, UTF_CONSOLE);

/* Test that the length is a count of values and carries over to the next md */
static int cmd_test_md_length(struct unit_test_state *uts)
{
	int i;

	if (gd->ram_base > MD_ADDR)
		return -EAGAIN;
	setup_mem();

	/* 0x14 longs are 0x50 bytes, which needs five lines */
	ut_assertok(run_commandf("md.l %lx 14", MD_ADDR));
	for (i = 0; i < 5; i++)
		ut_assertok(check_line(uts, MD_ADDR + i * 0x10, LINE_L));
	ut_assert_console_end();

	/*
	 * the length is remembered, so a command without one shows the same
	 * number of values again. The size is not: it comes from the suffix
	 * each time, defaulting to a long
	 */
	ut_assertok(run_commandf("md.b %lx 10", MD_ADDR));
	ut_assertok(check_line(uts, MD_ADDR, LINE_B));
	ut_assertok(run_commandf("md %lx", MD_ADDR));
	for (i = 0; i < 4; i++)
		ut_assertok(check_line(uts, MD_ADDR + i * 0x10, LINE_L));
	ut_assert_console_end();

	/*
	 * put the remembered length back, since it is the default for any
	 * later md which does not give one
	 */
	ut_assertok(run_commandf("md.l %lx 40", MD_ADDR));
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_md_length, UTF_CONSOLE);

/* Test that repeating 'md' carries on where the last one stopped */
static int cmd_test_md_repeat(struct unit_test_state *uts)
{
	char *const args[] = { "md.l", NULL, "4", NULL };
	char addr_str[20];
	int repeatable;

	if (gd->ram_base > MD_ADDR)
		return -EAGAIN;
	setup_mem();

	snprintf(addr_str, sizeof(addr_str), "%lx", MD_ADDR);
	((char **)args)[1] = addr_str;

	/* a fresh command starts at the address it is given */
	ut_assertok(cmd_process(0, 3, args, &repeatable, NULL));
	ut_assertok(check_line(uts, MD_ADDR, LINE_L));
	ut_assert_console_end();

	/*
	 * A repeat takes no notice of the arguments and shows the next block,
	 * which it can only do by seeing CMD_FLAG_REPEAT in the state
	 */
	ut_assertok(cmd_process(CMD_FLAG_REPEAT, 3, args, &repeatable, NULL));
	ut_assertok(check_line(uts, MD_ADDR + 0x10, LINE_L));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_md_repeat, UTF_CONSOLE);

/* Test 'md' with a bad command line */
static int cmd_test_md_usage(struct unit_test_state *uts)
{
	if (gd->ram_base > MD_ADDR)
		return -EAGAIN;
	setup_mem();

	/* an address is required */
	ut_asserteq(1, run_command("md", 0));
	ut_assert_nextline("md - memory display");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_linen("md [.b, .w, .l");
	ut_assert_console_end();

	/* at most two arguments are accepted */
	ut_asserteq(1, run_commandf("md %lx 4 4", MD_ADDR));
	ut_assert_nextline("md - memory display");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_linen("md [.b, .w, .l");
	ut_assert_console_end();

	/* an unknown size fails without a message */
	ut_asserteq(1, run_commandf("md.x %lx 4", MD_ADDR));
	ut_assert_console_end();

	/* so does a string, which only the ms command understands */
	ut_asserteq(1, run_commandf("md.s %lx 4", MD_ADDR));
	ut_assert_console_end();

	/*
	 * the command has no options, so one is refused rather than read as an
	 * address, which is what it meant before the getopt conversion
	 */
	ut_asserteq(1, run_command("md -1 4", 0));
	ut_assert_nextline("md - memory display");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_linen("md [.b, .w, .l");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_md_usage, UTF_CONSOLE);
