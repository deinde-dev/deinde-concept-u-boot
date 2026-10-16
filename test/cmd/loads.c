// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the loads command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <env.h>
#include <errno.h>
#include <mapmem.h>
#include <asm/global_data.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * The load address is part of the record text, so it is a constant: the S3
 * records below name 0x1000 and the S1 records 0x100. Only sandbox has memory
 * there, since qemu_arm and the riscv boards map NOR flash at the bottom of
 * the address space and qemu-arm-sbsa maps nothing at all. Every test which
 * stores data therefore passes an offset, to move the load into the region
 * the board keeps for loading images.
 */
#define LOADS_OFF	((ulong)CONFIG_SYS_LOAD_ADDR)

/* address the eight bytes of the S3 record end up at */
#define LOADS_ADDR	(LOADS_OFF + 0x1000)

/* offset used with the S1 records, which moves the load further along */
#define LOADS_OFF16	(LOADS_OFF + 0x2000)

/* address the four bytes of the S1 record end up at */
#define LOADS_ADDR16	(LOADS_OFF16 + 0x100)

/* the eight data bytes the S3 record carries */
static const u8 srec_data[] = { 0xde, 0xad, 0xbe, 0xef,
				0x0b, 0xad, 0xc0, 0xde };

/* the four the S1 record carries */
static const u8 srec_data16[] = { 0xc0, 0xff, 0xee, 0x11 };

/*
 * one S3 data record of eight bytes at 0x1000, an S5 count record and an S7
 * end record giving 0x1000 as the start address
 */
static const char srec_good[] =
	"S30D00001000DEADBEEF0BADC0DE54\n"
	"S5030001FB\n"
	"S70500001000EA\n";

/* the same records with the 16-bit address form: S1 data and S9 end */
static const char srec_good16[] =
	"S1070100C0FFEE1139\n"
	"S9030100FB\n";

/* the S3 record with the last digit of its checksum changed */
static const char srec_bad[] =
	"S30D00001000DEADBEEF0BADC0DE55\n";

/*
 * loads_can_store() - check that the offset reaches memory the test can use
 *
 * The offset is CONFIG_SYS_LOAD_ADDR, which a board with no default load
 * address leaves at 0. There is then no address known to hold usable memory,
 * as on qemu-arm-sbsa, whose memory starts above 4GB.
 */
static bool loads_can_store(void)
{
	return gd->ram_base <= LOADS_ADDR;
}

/*
 * Each test sets loads_echo for itself, since CONFIG_LOADS_ECHO decides what
 * the environment starts with and the echo lands in the console output.
 *
 * The command flushes the cache over the bytes it loaded before the caller
 * prints the start address. On ARM a range which is not cache-line aligned
 * prints 'CACHE: Misaligned operation at range ...' there, and the four and
 * eight bytes these records carry never are, so the tests skip to the
 * start-address line rather than require it to come next.
 */

/* Test 'loads' with a small S-record file */
static int cmd_test_loads_base(struct unit_test_state *uts)
{
	u8 *ptr;

	if (!loads_can_store())
		return -EAGAIN;

	ut_assertok(env_set("loads_echo", "0"));
	ptr = map_sysmem(LOADS_ADDR, sizeof(srec_data));
	memset(ptr, '\0', sizeof(srec_data));

	console_in_puts(srec_good);
	ut_assertok(run_commandf("loads %lx", LOADS_OFF));
	ut_assert_nextline("## Ready for S-Record download ...");
	ut_assert_nextline_empty();
	ut_assert_nextline("## First Load Addr = 0x%08lX", LOADS_ADDR);
	ut_assert_nextline("## Last  Load Addr = 0x%08lX", LOADS_ADDR + 7);
	ut_assert_nextline("## Total Size      = 0x00000008 = 8 Bytes");
	ut_assert_skip_to_line("## Start Addr      = 0x00001000");
	ut_assert_console_end();

	ut_asserteq_mem(srec_data, ptr, sizeof(srec_data));
	ut_asserteq_str("8", env_get("filesize"));
	unmap_sysmem(ptr);

	return 0;
}
CMD_TEST(cmd_test_loads_base, UTF_CONSOLE);

/* Test that the offset moves the data but not the reported start address */
static int cmd_test_loads_offset(struct unit_test_state *uts)
{
	u8 *ptr;

	if (!loads_can_store())
		return -EAGAIN;

	ut_assertok(env_set("loads_echo", "0"));
	ptr = map_sysmem(LOADS_ADDR16, sizeof(srec_data16));
	memset(ptr, '\0', sizeof(srec_data16));

	console_in_puts(srec_good16);
	ut_assertok(run_commandf("loads %lx", LOADS_OFF16));
	ut_assert_nextline("## Ready for S-Record download ...");
	ut_assert_nextline_empty();
	ut_assert_nextline("## First Load Addr = 0x%08lX", LOADS_ADDR16);
	ut_assert_nextline("## Last  Load Addr = 0x%08lX", LOADS_ADDR16 + 3);
	ut_assert_nextline("## Total Size      = 0x00000004 = 4 Bytes");

	/* the start address comes from the end record and ignores the offset */
	ut_assert_skip_to_line("## Start Addr      = 0x00000100");
	ut_assert_console_end();

	ut_asserteq_mem(srec_data16, ptr, sizeof(srec_data16));
	ut_asserteq_str("4", env_get("filesize"));
	unmap_sysmem(ptr);

	return 0;
}
CMD_TEST(cmd_test_loads_offset, UTF_CONSOLE);

/* Test that loads_echo sends every character received back to the console */
static int cmd_test_loads_echo(struct unit_test_state *uts)
{
	if (!loads_can_store())
		return -EAGAIN;

	ut_assertok(env_set("loads_echo", "1"));

	console_in_puts(srec_good);
	ut_assertok(run_commandf("loads %lx", LOADS_OFF));
	ut_assert_nextline("## Ready for S-Record download ...");
	ut_assert_nextline("S30D00001000DEADBEEF0BADC0DE54");
	ut_assert_nextline("S5030001FB");
	ut_assert_nextline("S70500001000EA");
	ut_assert_nextline_empty();
	ut_assert_nextline("## First Load Addr = 0x%08lX", LOADS_ADDR);
	ut_assert_nextline("## Last  Load Addr = 0x%08lX", LOADS_ADDR + 7);
	ut_assert_nextline("## Total Size      = 0x00000008 = 8 Bytes");
	ut_assert_skip_to_line("## Start Addr      = 0x00001000");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_loads_echo, UTF_CONSOLE);

/* Test that an option is refused rather than taken as an offset */
static int cmd_test_loads_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("loads -x", 0));
	ut_assert_nextline("loads - load S-Record file over serial line");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	if (IS_ENABLED(CONFIG_SYS_LOADS_BAUD_CHANGE))
		ut_assert_nextline("loads [ off ] [ baud ]");
	else
		ut_assert_nextline("loads [ off ]");
	ut_assert_nextlinen("    - load S-Record file over serial line");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_loads_usage, UTF_CONSOLE);

/* Test that a bad checksum aborts the download */
static int cmd_test_loads_bad(struct unit_test_state *uts)
{
	ut_assertok(env_set("loads_echo", "0"));

	console_in_puts(srec_bad);
	ut_asserteq(1, run_command("loads", 0));
	ut_assert_nextline("## Ready for S-Record download ...");
	ut_assert_nextline("## S-Record download aborted");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_loads_bad, UTF_CONSOLE);
