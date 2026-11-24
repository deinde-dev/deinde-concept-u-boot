// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the cp command
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
 * Memory these tests copy between, with the source at the start and the
 * target 0x40 bytes in. A board with no default load address leaves
 * CONFIG_SYS_LOAD_ADDR at 0 and qemu-arm-sbsa has no memory below 4GB, so each
 * test checks this against the start of memory.
 */
#define CP_ADDR		((ulong)CONFIG_SYS_LOAD_ADDR + 0x7000)
#define CP_SIZE		0x80
#define CP_DEST		(CP_ADDR + 0x40)

/* Value written over the target, so a copy into it can be seen */
#define CP_FILL		0xffffffff

/*
 * Fill the source with a pattern and the target with a known value
 *
 * @buf: Mapped region, of CP_SIZE bytes
 */
static void setup_mem(u32 *buf)
{
	int i;

	for (i = 0; i < CP_SIZE / sizeof(u32); i++)
		buf[i] = i < 0x10 ? 0x11223300 + i : CP_FILL;
}

/* Test 'cp' copying a run of values, which are longs by default */
static int cmd_test_cp_base(struct unit_test_state *uts)
{
	u32 *buf;
	int i;

	if (gd->ram_base > CP_ADDR)
		return -EAGAIN;

	buf = map_sysmem(CP_ADDR, CP_SIZE);
	setup_mem(buf);

	/* the copy says nothing; the data is the only evidence */
	ut_assertok(run_commandf("cp %lx %lx 4", CP_ADDR, CP_DEST));
	ut_assert_console_end();

	for (i = 0; i < 4; i++)
		ut_asserteq(0x11223300 + i, buf[0x10 + i]);
	ut_asserteq(CP_FILL, buf[0x14]);
	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_cp_base, UTF_CONSOLE);

/* Test that the count is a number of values, not of bytes */
static int cmd_test_cp_count(struct unit_test_state *uts)
{
	u32 *buf;

	if (gd->ram_base > CP_ADDR)
		return -EAGAIN;

	buf = map_sysmem(CP_ADDR, CP_SIZE);
	setup_mem(buf);

	/* four bytes are four bytes, so only the first long arrives */
	ut_assertok(run_commandf("cp.b %lx %lx 4", CP_ADDR, CP_DEST));
	ut_asserteq(0x11223300, buf[0x10]);
	ut_asserteq(CP_FILL, buf[0x11]);

	/* four longs are 0x10 bytes */
	setup_mem(buf);
	ut_assertok(run_commandf("cp.l %lx %lx 4", CP_ADDR, CP_DEST));
	ut_asserteq(0x11223303, buf[0x13]);
	ut_asserteq(CP_FILL, buf[0x14]);

	/* three halfwords are six bytes, so the seventh is left alone */
	setup_mem(buf);
	ut_assertok(run_commandf("cp.w %lx %lx 3", CP_ADDR, CP_DEST));
	ut_asserteq(0, memcmp(&buf[0x10], buf, 6));
	ut_asserteq(0xff, ((u8 *)&buf[0x10])[6]);
	ut_assert_console_end();
	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_cp_count, UTF_CONSOLE);

/* Test 'cp' with a bad command line */
static int cmd_test_cp_usage(struct unit_test_state *uts)
{
	u32 *buf;

	if (gd->ram_base > CP_ADDR)
		return -EAGAIN;

	buf = map_sysmem(CP_ADDR, CP_SIZE);
	setup_mem(buf);

	/* the count is required */
	ut_asserteq(1, run_commandf("cp %lx %lx", CP_ADDR, CP_DEST));
	ut_assert_nextline("cp - memory copy");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_linen("cp [.b, .w, .l");
	ut_assert_console_end();

	/* there is nothing to do for a count of zero, so it is refused */
	ut_asserteq(1, run_commandf("cp %lx %lx 0", CP_ADDR, CP_DEST));
	ut_assert_nextline("Zero length ???");
	ut_assert_console_end();

	/* an unknown size fails without a message */
	ut_asserteq(1, run_commandf("cp.x %lx %lx 4", CP_ADDR, CP_DEST));
	ut_assert_console_end();

	/* the target is untouched by any of them */
	ut_asserteq(CP_FILL, buf[0x10]);
	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_cp_usage, UTF_CONSOLE);
