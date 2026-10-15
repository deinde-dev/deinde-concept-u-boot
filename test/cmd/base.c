// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the base command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <mapmem.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Address of the region the offset test writes to */
#define BASE_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x1000)

/* Offset the tests set, and the number of 32-bit words they work with */
#define TEST_OFFSET	0x1234
#define WORD_COUNT	4

/* Value written through the offset */
#define TEST_VALUE	0xdeadbeef

/* Test 'base' reporting and setting the address offset */
static int cmd_test_base_base(struct unit_test_state *uts)
{
	/*
	 * The offset starts at zero and every test here puts it back, so the
	 * default can be checked whichever test runs first
	 */
	ut_assertok(run_command("base", 0));
	ut_assertok(run_commandf("base %x", TEST_OFFSET));
	ut_assertok(run_command("base", 0));
	ut_assertok(run_command("base 0", 0));

	ut_assert_nextline("Base Address: 0x00000000");
	ut_assert_nextline("Base Address: 0x%08x", TEST_OFFSET);
	ut_assert_nextline("Base Address: 0x%08x", TEST_OFFSET);
	ut_assert_nextline("Base Address: 0x00000000");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_base_base, UTF_CONSOLE);

/* Test that the offset applies to the addresses the memory commands take */
static int cmd_test_base_offset(struct unit_test_state *uts)
{
	ulong addr = BASE_ADDR;
	u32 *buf;
	int i;

	buf = map_sysmem(addr, WORD_COUNT * sizeof(u32));
	memset(buf, '\0', WORD_COUNT * sizeof(u32));

	/* with the offset set, address 0 means the start of the region */
	ut_assertok(run_commandf("base %lx", addr));
	ut_assertok(run_commandf("mw.l 0 %x %x", TEST_VALUE, WORD_COUNT));
	ut_assertok(run_commandf("md.l 0 %x", WORD_COUNT));
	ut_assertok(run_command("base 0", 0));

	ut_assert_nextline("Base Address: 0x%08lx", addr);
	ut_assert_nextline("%08lx: deadbeef deadbeef deadbeef deadbeef  ................",
			   addr);
	ut_assert_nextline("Base Address: 0x00000000");
	ut_assert_console_end();

	/* the write really landed in the region */
	for (i = 0; i < WORD_COUNT; i++)
		ut_asserteq(TEST_VALUE, buf[i]);

	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_base_offset, UTF_CONSOLE);

/* Test 'base' with a bad command line */
static int cmd_test_base_usage(struct unit_test_state *uts)
{
	/* the command takes at most one argument */
	ut_asserteq(CMD_RET_FAILURE, run_command("base 1 2", 0));
	ut_assert_nextline("base - print or set address offset");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("base ");
	ut_assert_nextline("    - print address offset for memory commands");
	ut_assert_nextline("base off");
	ut_assert_nextline("    - set address offset for memory commands to 'off'");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_base_usage, UTF_CONSOLE);
