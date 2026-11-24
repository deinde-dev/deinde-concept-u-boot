// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the cmp command
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
 * Memory these tests compare: two regions of CMP_LEN bytes, one after the
 * other. A board with no default load address leaves CONFIG_SYS_LOAD_ADDR at 0
 * and qemu-arm-sbsa has no memory below 4GB, so each test checks this against
 * the start of memory.
 */
#define CMP_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x8000)
#define CMP_LEN		0x20
#define CMP_DEST	(CMP_ADDR + CMP_LEN)

/*
 * Byte both regions are filled with, and the one which replaces a run of
 * DIFF_LEN bytes at DIFF_OFFSET in the second region. Every byte of a value
 * differs, so the first difference is at the same offset whatever the size,
 * and each value reads the same whichever way round the target stores it
 */
#define CMP_BYTE	0xef
#define DIFF_BYTE	0x12
#define DIFF_OFFSET	8
#define DIFF_LEN	8

/* Fill both regions with the same bytes, so a comparison finds them equal */
static void setup_mem(u8 *buf)
{
	memset(buf, CMP_BYTE, CMP_LEN * 2);
}

/* Make the second region differ from the first, from DIFF_OFFSET onwards */
static void setup_diff(u8 *buf)
{
	setup_mem(buf);
	memset(buf + CMP_LEN + DIFF_OFFSET, DIFF_BYTE, DIFF_LEN);
}

/*
 * check_diff() - Check the line cmp shows for the first difference
 *
 * @uts: Test state
 * @type: Name cmp gives to the size it is comparing
 * @val1: Value it shows for the first region
 * @val2: Value it shows for the second region
 * Return: 0 if OK, other value on error
 */
static int check_diff(struct unit_test_state *uts, const char *type,
		      const char *val1, const char *val2)
{
	ut_assert_nextline("%s at 0x%08lx (%s) != %s at 0x%08lx (%s)", type,
			   CMP_ADDR + DIFF_OFFSET, val1, type,
			   CMP_DEST + DIFF_OFFSET, val2);

	return 0;
}

/* Test 'cmp' finding two regions the same, which are longs by default */
static int cmd_test_cmp_base(struct unit_test_state *uts)
{
	u8 *buf;

	if (gd->ram_base > CMP_ADDR)
		return -EAGAIN;

	buf = map_sysmem(CMP_ADDR, CMP_LEN * 2);
	setup_mem(buf);

	ut_assertok(run_commandf("cmp %lx %lx 2", CMP_ADDR, CMP_DEST));
	ut_assert_nextline("Total of 2 word(s) were the same");
	ut_assert_console_end();
	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_cmp_base, UTF_CONSOLE);

/* Test that 'cmp' reports the first difference and stops there */
static int cmd_test_cmp_diff(struct unit_test_state *uts)
{
	u8 *buf;

	if (gd->ram_base > CMP_ADDR)
		return -EAGAIN;

	buf = map_sysmem(CMP_ADDR, CMP_LEN * 2);
	setup_diff(buf);

	/* the count covers the difference, so the command fails */
	ut_asserteq(1, run_commandf("cmp %lx %lx 4", CMP_ADDR, CMP_DEST));
	ut_assertok(check_diff(uts, "word", "0xefefefef", "0x12121212"));
	ut_assert_nextline("Total of 2 word(s) were the same");
	ut_assert_console_end();

	/* a count which stops short of it sees no difference */
	ut_assertok(run_commandf("cmp %lx %lx 2", CMP_ADDR, CMP_DEST));
	ut_assert_nextline("Total of 2 word(s) were the same");
	ut_assert_console_end();
	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_cmp_diff, UTF_CONSOLE);

/* Test each data size, which names the values it compares */
static int cmd_test_cmp_size(struct unit_test_state *uts)
{
	u8 *buf;

	if (gd->ram_base > CMP_ADDR)
		return -EAGAIN;

	buf = map_sysmem(CMP_ADDR, CMP_LEN * 2);
	setup_diff(buf);

	/* the count is a number of values, so each of these covers 0x10 bytes */
	ut_asserteq(1, run_commandf("cmp.b %lx %lx 10", CMP_ADDR, CMP_DEST));
	ut_assertok(check_diff(uts, "byte", "0xef", "0x12"));
	ut_assert_nextline("Total of 8 byte(s) were the same");

	ut_asserteq(1, run_commandf("cmp.w %lx %lx 8", CMP_ADDR, CMP_DEST));
	ut_assertok(check_diff(uts, "halfword", "0xefef", "0x1212"));
	ut_assert_nextline("Total of 4 halfword(s) were the same");

	ut_asserteq(1, run_commandf("cmp.l %lx %lx 4", CMP_ADDR, CMP_DEST));
	ut_assertok(check_diff(uts, "word", "0xefefefef", "0x12121212"));
	ut_assert_nextline("Total of 2 word(s) were the same");

	if (MEM_SUPPORT_64BIT_DATA) {
		ut_asserteq(1, run_commandf("cmp.q %lx %lx 2", CMP_ADDR,
					    CMP_DEST));
		ut_assertok(check_diff(uts, "double word",
				       "0xefefefefefefefef",
				       "0x1212121212121212"));
		ut_assert_nextline("Total of 1 double word(s) were the same");
	}
	ut_assert_console_end();
	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_cmp_size, UTF_CONSOLE);

/* Test 'cmp' with a bad command line */
static int cmd_test_cmp_usage(struct unit_test_state *uts)
{
	u8 *buf;

	if (gd->ram_base > CMP_ADDR)
		return -EAGAIN;

	buf = map_sysmem(CMP_ADDR, CMP_LEN * 2);
	setup_mem(buf);

	/* the count is required */
	ut_asserteq(1, run_commandf("cmp %lx %lx", CMP_ADDR, CMP_DEST));
	ut_assert_nextline("cmp - memory compare");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_linen("cmp [.b, .w, .l");
	ut_assert_console_end();

	/* an unknown size fails without a message */
	ut_asserteq(1, run_commandf("cmp.x %lx %lx 2", CMP_ADDR, CMP_DEST));
	ut_assert_console_end();

	/* so does a string, which only the ms command understands */
	ut_asserteq(1, run_commandf("cmp.s %lx %lx 2", CMP_ADDR, CMP_DEST));
	ut_assert_console_end();

	/*
	 * the command has no options, so one is refused rather than read as an
	 * address, which is what it meant before the getopt conversion
	 */
	ut_asserteq(1, run_commandf("cmp -1 %lx 2", CMP_DEST));
	ut_assert_nextline("cmp - memory compare");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_linen("cmp [.b, .w, .l");
	ut_assert_console_end();
	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_cmp_usage, UTF_CONSOLE);
