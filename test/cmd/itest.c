// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the itest command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <compiler.h>
#include <console.h>
#include <env.h>
#include <mapmem.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Address of the region holding the values the tests point at */
#define ITEST_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x1000)

/* Size of that region */
#define ITEST_SIZE	0x20

/* Offset within it of the string the pointer test compares, and that string */
#define STR_OFFSET	0x10
#define TEST_STR	"hello"

/* The data widths the command offers, which depend on the build */
#if MEM_SUPPORT_64BIT_DATA
#define WIDTHS		"[.b, .w, .l, .q, .s]"
#else
#define WIDTHS		"[.b, .w, .l, .s]"
#endif

/* Test 'itest' comparing two numbers */
static int cmd_test_itest_base(struct unit_test_state *uts)
{
	/* every operator has a word spelling and at least one symbol */
	ut_assertok(run_command("itest 3 -lt 4", 0));
	ut_assertok(run_command("itest 3 < 4", 0));
	ut_assertok(run_command("itest 4 -gt 3", 0));
	ut_assertok(run_command("itest 4 > 3", 0));
	ut_assertok(run_command("itest 3 -eq 3", 0));
	ut_assertok(run_command("itest 3 == 3", 0));
	ut_assertok(run_command("itest 3 -ne 4", 0));
	ut_assertok(run_command("itest 3 != 4", 0));
	ut_assertok(run_command("itest 3 <> 4", 0));
	ut_assertok(run_command("itest 3 -ge 3", 0));
	ut_assertok(run_command("itest 3 >= 3", 0));
	ut_assertok(run_command("itest 3 -le 3", 0));
	ut_assertok(run_command("itest 3 <= 3", 0));

	/* the command says nothing either way */
	ut_assert_console_end();

	/* a comparison which is false fails, which is how $? becomes 1 */
	ut_asserteq(CMD_RET_FAILURE, run_command("itest 3 -gt 4", 0));
	ut_asserteq(CMD_RET_FAILURE, run_command("itest 3 == 4", 0));
	ut_asserteq(CMD_RET_FAILURE, run_command("itest 3 -le 2", 0));
	ut_assert_console_end();

	/* the values are hexadecimal, so this compares 16 against 9 */
	ut_assertok(run_command("itest 10 -gt 9", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_itest_base, UTF_CONSOLE);

/* Test that the postfix truncates the values before they are compared */
static int cmd_test_itest_width(struct unit_test_state *uts)
{
	/* .l keeps all four bytes, so 0x3002 is below 0x4001 */
	ut_assertok(run_command("itest.l 3002 < 4001", 0));

	/* .w keeps the low two, which does not change the answer here */
	ut_assertok(run_command("itest.w 3002 < 4001", 0));

	/* .b keeps only the low byte, leaving 2 against 1 */
	ut_assertok(run_command("itest.b 3002 > 4001", 0));

	/* the default is .l */
	ut_assertok(run_command("itest 3002 < 4001", 0));

	/* truncation applies to both values, so these are equal as bytes */
	ut_assertok(run_command("itest.b 1100 == 2200", 0));
	ut_asserteq(CMD_RET_FAILURE, run_command("itest.w 1100 == 2200", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_itest_width, UTF_CONSOLE);

/* Test reading the values to compare out of memory */
static int cmd_test_itest_ptr(struct unit_test_state *uts)
{
	ulong addr = ITEST_ADDR;
	u8 *buf;

	buf = map_sysmem(addr, ITEST_SIZE);
	memset(buf, '\0', ITEST_SIZE);

	/* write each width through a pointer, so endianness does not matter */
	*(u8 *)(buf + 0) = 0x12;
	*(u16 *)(buf + 2) = 0x1234;
	*(u32 *)(buf + 4) = 0x12345678;

	ut_assertok(run_commandf("itest.b *%lx == 12", addr));
	ut_assertok(run_commandf("itest.w *%lx == 1234", addr + 2));
	ut_assertok(run_commandf("itest.l *%lx == 12345678", addr + 4));

	/* the comparison is against the value in memory, not the address */
	ut_asserteq(CMD_RET_FAILURE,
		    run_commandf("itest.l *%lx == %lx", addr + 4, addr + 4));

	if (MEM_SUPPORT_64BIT_DATA) {
		*(u64 *)(buf + 8) = 0x123456789abcdef0;
		ut_assertok(run_commandf("itest.q *%lx == 123456789abcdef0",
					 addr + 8));
	}

	/* a string comparison reads the bytes at the address as a string */
	memcpy(buf + STR_OFFSET, TEST_STR, sizeof(TEST_STR));
	ut_assertok(run_commandf("itest.s *%lx == " TEST_STR,
				 addr + STR_OFFSET));
	ut_asserteq(CMD_RET_FAILURE,
		    run_commandf("itest.s *%lx == " TEST_STR "x",
				 addr + STR_OFFSET));
	ut_assert_console_end();

	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_itest_ptr, UTF_CONSOLE);

/* Test 'itest.s' comparing strings and environment variables */
static int cmd_test_itest_str(struct unit_test_state *uts)
{
	/* a string comparison is alphabetical, not numeric */
	ut_assertok(run_command("itest.s 0xa1234 < 0xb", 0));
	ut_asserteq(CMD_RET_FAILURE, run_command("itest 0xa1234 < 0xb", 0));

	/* an empty string is below anything else */
	ut_assertok(run_command("itest.s '' < a", 0));

	/*
	 * '${name}' is the value of an environment variable. The quotes keep
	 * the shell from expanding it before the command sees it
	 */
	ut_assertok(env_set("itest_var", "hello"));
	ut_assertok(run_command("itest.s '${itest_var}' == hello", 0));
	ut_asserteq(CMD_RET_FAILURE,
		    run_command("itest.s '${itest_var}' == goodbye", 0));

	/* a variable which is not set compares as an empty string */
	ut_assertok(run_command("itest.s '${itest_none}' == ''", 0));
	ut_assert_console_end();

	/* anything but '${name}' after a '$' is refused */
	ut_asserteq(CMD_RET_FAILURE,
		    run_command("itest.s '$itest_var' == hello", 0));
	ut_assert_nextline("Invalid variable reference");
	ut_assert_console_end();

	/* so is a reference with no closing brace */
	ut_asserteq(CMD_RET_FAILURE,
		    run_command("itest.s '${itest_var' == hello", 0));
	ut_assert_nextline("Invalid variable reference");
	ut_assert_console_end();

	ut_assertok(env_set("itest_var", NULL));

	return 0;
}
CMD_TEST(cmd_test_itest_str, UTF_CONSOLE);

/* Check the usage message the command prints when it refuses a command line */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("itest - return true/false on integer compare");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("itest " WIDTHS " [*]value1 <op> [*]value2");
	ut_assert_console_end();

	return 0;
}

/* Test 'itest' with a bad command line */
static int cmd_test_itest_usage(struct unit_test_state *uts)
{
	/* the command wants two values and an operator, no more and no less */
	ut_asserteq(CMD_RET_FAILURE, run_command("itest", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(CMD_RET_FAILURE, run_command("itest 1 ==", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(CMD_RET_FAILURE, run_command("itest 1 == 1 2", 0));
	ut_assertok(check_usage(uts));

	/* the command has no options, so one is refused rather than read */
	ut_asserteq(CMD_RET_FAILURE, run_command("itest -1 < 4", 0));
	ut_assertok(check_usage(uts));

	/* an operator which is not in the table is reported */
	ut_asserteq(CMD_RET_FAILURE, run_command("itest 1 lt 2", 0));
	ut_assert_nextline("Unknown operator 'lt'");
	ut_assert_console_end();

	/* so is a postfix which is not one of the known widths */
	ut_asserteq(CMD_RET_FAILURE, run_command("itest.x 1 == 1", 0));
	ut_assert_nextline("Invalid data width specifier");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_itest_usage, UTF_CONSOLE);
