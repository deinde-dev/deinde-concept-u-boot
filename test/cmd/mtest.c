// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the mtest command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <mapmem.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Address of the region the tests ask mtest to cover */
#define MTEST_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x1000)

/* Number of longs the tests ask mtest to cover */
#define WORD_COUNT	4

/* Pattern the pattern test asks for */
#define TEST_PATTERN	0x55aa

/*
 * mem_test_quick() pads its 'Writing...' message to the width of
 * 'Reading...' and then backs up over the padding
 */
#define PAD		"            \b\b\b\b\b\b\b\b\b\b"

/* One iteration of the quick test, as it appears on a single console line */
#define ITERATION	"Iteration: %6d\r\rPattern %0*lX  Writing..." PAD \
			"Reading..."

/* Clear the region a test works on and return a pointer to it */
static ulong *clear_buf(void)
{
	ulong *buf;

	buf = map_sysmem(MTEST_ADDR, WORD_COUNT * sizeof(ulong));
	memset(buf, '\0', WORD_COUNT * sizeof(ulong));

	return buf;
}

/* Test 'mtest' over a small region, for a fixed number of iterations */
static int cmd_test_mtest_base(struct unit_test_state *uts)
{
	const int plen = 2 * sizeof(ulong);
	ulong *buf, addr, size;
	int i;

	/* the alternative test reports differently and has no pattern */
	if (IS_ENABLED(CONFIG_SYS_ALT_MEMTEST))
		return -EAGAIN;

	buf = clear_buf();
	addr = MTEST_ADDR;
	size = WORD_COUNT * sizeof(ulong);

	ut_assertok(run_commandf("mtest %lx %lx 0 1", addr, addr + size));

	ut_assert_nextline("Testing %08lx ... %08lx:", addr, addr + size);
	ut_assert_nextline(ITERATION, 1, plen, 0UL);
	ut_assert_nextline("Tested 1 iteration(s) with 0 errors.");
	ut_assert_console_end();

	/* the test leaves the pattern it wrote, incremented along the region */
	for (i = 0; i < WORD_COUNT; i++)
		ut_asserteq(i, buf[i]);

	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_mtest_base, UTF_CONSOLE);

/* Test that 'mtest' alternates the pattern from one iteration to the next */
static int cmd_test_mtest_pattern(struct unit_test_state *uts)
{
	const int plen = 2 * sizeof(ulong);
	ulong *buf, addr, size;
	int i;

	if (IS_ENABLED(CONFIG_SYS_ALT_MEMTEST))
		return -EAGAIN;

	buf = clear_buf();
	addr = MTEST_ADDR;
	size = WORD_COUNT * sizeof(ulong);

	/* the arguments are hexadecimal, so this asks for two iterations */
	ut_assertok(run_commandf("mtest %lx %lx %x 2", addr, addr + size,
				 TEST_PATTERN));

	ut_assert_nextline("Testing %08lx ... %08lx:", addr, addr + size);

	/*
	 * The second iteration complements the pattern and counts down
	 * instead of up, so that memory sees plenty of ones as well as zeros
	 */
	ut_assert_nextline(ITERATION ITERATION, 1, plen, (ulong)TEST_PATTERN,
			   2, plen, ~(ulong)TEST_PATTERN);
	ut_assert_nextline("Tested 2 iteration(s) with 0 errors.");
	ut_assert_console_end();

	for (i = 0; i < WORD_COUNT; i++)
		ut_asserteq_64(~(ulong)TEST_PATTERN - i, buf[i]);

	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_mtest_pattern, UTF_CONSOLE);

/* Test that Ctrl-C stops a run which has no iteration limit */
static int cmd_test_mtest_ctrlc(struct unit_test_state *uts)
{
	ulong addr, size;
	int prev;

	if (IS_ENABLED(CONFIG_SYS_ALT_MEMTEST))
		return -EAGAIN;

	addr = MTEST_ADDR;
	size = WORD_COUNT * sizeof(ulong);

	/*
	 * Without an iteration count the command runs for ever, so queue up a
	 * Ctrl-C before starting it. Sandbox turns off Ctrl-C checking, so
	 * enable it for the duration of the command
	 */
	console_in_puts("\x03");
	prev = disable_ctrlc(0);
	ut_asserteq(CMD_RET_FAILURE,
		    run_commandf("mtest %lx %lx", addr, addr + size));
	disable_ctrlc(prev);

	/* the Ctrl-C is noticed before the first iteration starts */
	ut_assert_nextline("Testing %08lx ... %08lx:", addr, addr + size);
	ut_assert_nextline_empty();
	ut_assert_nextline("Tested 0 iteration(s) with 0 errors.");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mtest_ctrlc, UTF_CONSOLE);

/* Test 'mtest' with a bad command line */
static int cmd_test_mtest_usage(struct unit_test_state *uts)
{
	/* an end address below the start one is refused, without the usage */
	ut_asserteq(CMD_RET_FAILURE, run_command("mtest 2000 1000", 0));
	ut_assert_nextline("Refusing to do empty test");
	ut_assert_console_end();

	/* each argument must be a hexadecimal number */
	ut_asserteq(CMD_RET_FAILURE, run_command("mtest zz", 0));
	ut_assert_nextline("mtest - simple RAM read/write test");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("mtest [start [end [pattern [iterations]]]]");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mtest_usage, UTF_CONSOLE);
