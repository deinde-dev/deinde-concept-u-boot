// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for 'meminfo' command
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <dm/test.h>
#include <env.h>
#include <mapmem.h>
#include <asm/global_data.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/* Test 'meminfo' command */
static int cmd_test_meminfo_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("meminfo", 0));

	/* only sandbox builds this test, so the size is a whole MiB */
	ut_assert_nextline("DRAM:  %lu MiB", (ulong)(gd->ram_size >> 20));

	if (!IS_ENABLED(CONFIG_CMD_MEMINFO_MAP))
		return 0;

	ut_assert_nextline_empty();

	ut_assert_nextline("Region                Base          Size           End           Gap");
	ut_assert_nextlinen("-");

	/*
	 * check the name, base and size of each region which the test can
	 * work out for itself; for the others only the name
	 */
	if (IS_ENABLED(CONFIG_VIDEO))
		ut_assert_nextlinen("video");
	if (IS_ENABLED(CONFIG_TRACE))
		ut_assert_nextlinen("trace");
	ut_assert_nextlinen("%-12s %13lx %13lx %13lx", "code", gd->relocaddr,
			    (ulong)gd->mon_len, gd->relocaddr + gd->mon_len);
	ut_assert_nextlinen("malloc");
	ut_assert_nextlinen("%-12s %13lx %13lx", "board_info",
			    (ulong)map_to_sysmem(gd->bd),
			    (ulong)sizeof(struct bd_info));
	ut_assert_nextlinen("%-12s %13lx %13lx", "global_data",
			    (ulong)map_to_sysmem((void *)gd),
			    (ulong)sizeof(struct global_data));
	ut_assert_nextlinen("devicetree");
	if (IS_ENABLED(CONFIG_BOOTSTAGE))
		ut_assert_nextlinen("bootstage");
	if (IS_ENABLED(CONFIG_BLOBLIST))
		ut_assert_nextlinen("bloblist");
	ut_assert_nextlinen("%-12s %13lx %13lx", "stack",
			    gd->start_addr_sp - CONFIG_STACK_SIZE,
			    (ulong)CONFIG_STACK_SIZE);

	/* we expect at least one lmb line, but don't know how many */
	if (IS_ENABLED(CONFIG_LMB))
		ut_assert_nextlinen("lmb");
	ut_assert_skip_to_linen("%-12s %13lx", "free", gd->ram_base);

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_meminfo_base, UTF_CONSOLE);

/* Test that 'meminfo' rejects an argument */
static int cmd_test_meminfo_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("meminfo extra", 0));
	ut_assert_nextline("meminfo - display memory information");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextlinen("meminfo");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_meminfo_usage, UTF_CONSOLE);

/* Test 'memsize' command */
#ifdef CONFIG_CMD_MEMSIZE
static int cmd_test_memsize_base(struct unit_test_state *uts)
{
	ulong mib = gd->ram_size >> 20;

	ut_assertok(run_command("memsize", 0));
	ut_assert_nextline("%lu MiB", mib);
	ut_assert_console_end();

	/* with a name it writes the size to that variable and shows nothing */
	ut_assertok(run_command("memsize memsz", 0));
	ut_asserteq(mib, env_get_ulong("memsz", 10, 0));
	ut_assert_console_end();

	/* a second run overwrites the variable */
	ut_assertok(env_set("memsz", "nonsense"));
	ut_assertok(run_command("memsize memsz", 0));
	ut_asserteq(mib, env_get_ulong("memsz", 10, 0));
	ut_assert_console_end();

	/* leave the environment as it was found */
	ut_assertok(env_set("memsz", NULL));

	return 0;
}
CMD_TEST(cmd_test_memsize_base, UTF_CONSOLE);

/* Test 'memsize' with too many arguments */
static int cmd_test_memsize_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("memsize memsz extra", 0));
	ut_assert_nextlinen("memsize - get detected ram size in MiB");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("memsize [envvar]");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_memsize_usage, UTF_CONSOLE);
#endif
