// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the mw command
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
 * Memory used by these tests. A board with no default load address leaves
 * CONFIG_SYS_LOAD_ADDR at 0 and qemu-arm-sbsa has no memory below 4GB, so each
 * test checks this against the start of memory.
 */
#define MW_ADDR		((ulong)CONFIG_SYS_LOAD_ADDR + 0x5000)
#define MW_SIZE		0x20

/* Fill the region with a known value, so a write to it can be seen */
static void setup_mem(void)
{
	u8 *buf;

	buf = map_sysmem(MW_ADDR, MW_SIZE);
	memset(buf, 0xff, MW_SIZE);
	unmap_sysmem(buf);
}

/* Test 'mw' writing a single value, which is the default size */
static int cmd_test_mw_base(struct unit_test_state *uts)
{
	u32 *buf;

	if (gd->ram_base > MW_ADDR)
		return -EAGAIN;
	setup_mem();

	buf = map_sysmem(MW_ADDR, MW_SIZE);
	ut_assertok(run_commandf("mw %lx 12345678", MW_ADDR));
	ut_assert_console_end();

	/* only the first long is written */
	ut_asserteq(0x12345678, buf[0]);
	ut_asserteq(0xffffffff, buf[1]);

	/* the value is truncated to the size written */
	ut_assertok(run_commandf("mw.b %lx 12345678", MW_ADDR));
	ut_asserteq(0x78, *(u8 *)buf);
	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_mw_base, UTF_CONSOLE);

/* Test that a count fills a region */
static int cmd_test_mw_count(struct unit_test_state *uts)
{
	u32 *buf;
	int i;

	if (gd->ram_base > MW_ADDR)
		return -EAGAIN;
	setup_mem();

	buf = map_sysmem(MW_ADDR, MW_SIZE);
	ut_assertok(run_commandf("mw %lx 0 4", MW_ADDR));
	ut_assert_console_end();

	/* the count is in values, not bytes, so 4 longs are cleared */
	for (i = 0; i < 4; i++)
		ut_asserteq(0, buf[i]);
	ut_asserteq(0xffffffff, buf[4]);
	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_mw_count, UTF_CONSOLE);

/* Test each data size */
static int cmd_test_mw_size(struct unit_test_state *uts)
{
	u8 *buf;

	if (gd->ram_base > MW_ADDR)
		return -EAGAIN;
	setup_mem();

	buf = map_sysmem(MW_ADDR, MW_SIZE);
	ut_assertok(run_commandf("mw.b %lx 11 2", MW_ADDR));
	ut_asserteq(0x11, buf[0]);
	ut_asserteq(0x11, buf[1]);
	ut_asserteq(0xff, buf[2]);

	ut_assertok(run_commandf("mw.w %lx 2233 2", MW_ADDR + 4));
	ut_asserteq(0x2233, *(u16 *)(buf + 4));
	ut_asserteq(0x2233, *(u16 *)(buf + 6));
	ut_asserteq(0xff, buf[8]);

	ut_assertok(run_commandf("mw.l %lx 44556677 2", MW_ADDR + 0xc));
	ut_asserteq(0x44556677, *(u32 *)(buf + 0xc));
	ut_asserteq(0x44556677, *(u32 *)(buf + 0x10));
	ut_asserteq(0xff, buf[0x14]);

	if (MEM_SUPPORT_64BIT_DATA) {
		ut_assertok(run_commandf("mw.q %lx 8899aabbccddeeff",
					 MW_ADDR + 0x18));
		ut_asserteq_64(0x8899aabbccddeeffULL,
			       *(u64 *)(buf + 0x18));
	}
	ut_assert_console_end();
	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_mw_size, UTF_CONSOLE);

/* Test 'mw' with a bad command line */
static int cmd_test_mw_usage(struct unit_test_state *uts)
{
	if (gd->ram_base > MW_ADDR)
		return -EAGAIN;
	setup_mem();

	/* the value is required */
	ut_asserteq(1, run_commandf("mw %lx", MW_ADDR));
	ut_assert_nextline("mw - memory write (fill)");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_linen("mw [.b, .w, .l");
	ut_assert_console_end();

	/* at most three arguments are accepted */
	ut_asserteq(1, run_commandf("mw %lx 0 1 2", MW_ADDR));
	ut_assert_nextline("mw - memory write (fill)");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_linen("mw [.b, .w, .l");
	ut_assert_console_end();

	/* an unknown size fails without a message */
	ut_asserteq(1, run_commandf("mw.x %lx 0", MW_ADDR));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mw_usage, UTF_CONSOLE);
