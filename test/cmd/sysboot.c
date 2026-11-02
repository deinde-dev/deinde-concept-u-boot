// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the sysboot command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <env.h>
#include <test/cmd.h>
#include <test/ut.h>

/* First line of the usage message */
#define SYSBOOT_USAGE	"sysboot - command to get and boot from syslinux files"

/* Message shown when nothing says which file to read */
#define NO_FILENAME \
	"Specify a filename or set the ${bootfile} environment variable"

/* Check the usage message for too few arguments */
static int cmd_test_sysboot_base(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("sysboot", 0));
	ut_assert_nextline(SYSBOOT_USAGE);
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextlinen("sysboot [-p] <interface> <dev[:part]> ");
		ut_assert_nextlinen("    - load and parse syslinux menu file ");
		ut_assert_nextlinen("      or any filesystem on 'dev' ");
	}
	ut_assert_console_end();

	/* the interface, device and filesystem are all required */
	ut_asserteq(1, run_command("sysboot host", 0));
	ut_assert_nextline(SYSBOOT_USAGE);
	console_record_reset();

	ut_asserteq(1, run_command("sysboot host 0:0", 0));
	ut_assert_nextline(SYSBOOT_USAGE);
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_sysboot_base, UTF_CONSOLE);

/* Check the checks the command makes on its arguments */
static int cmd_test_sysboot_args(struct unit_test_state *uts)
{
	/*
	 * Set up both variables the command reads, since an earlier test may
	 * have left either of them unset
	 */
	ut_assertok(env_set_hex("pxefile_addr_r", CONFIG_SYS_LOAD_ADDR));
	ut_assertok(env_set("bootfile", NULL));

	/* with no filename and no bootfile there is nothing to read */
	ut_asserteq(1, run_command("sysboot host 0:0 any", 0));
	ut_assert_nextline(NO_FILENAME);
	ut_assert_console_end();

	/* a filesystem the command does not know about is rejected */
	ut_asserteq(1, run_commandf("sysboot host 0:0 ntfs %lx cfg",
				    (ulong)CONFIG_SYS_LOAD_ADDR));
	ut_assert_nextline("Invalid filesystem: ntfs");
	ut_assert_console_end();

	/* giving the filename sets bootfile, so it is now known */
	ut_asserteq_str("cfg", env_get("bootfile"));

	/* an address which is not a hex number is rejected */
	ut_asserteq(1, run_command("sysboot host 0:0 any zzz cfg", 0));
	ut_assert_nextline("Invalid pxefile address: zzz");
	ut_assert_console_end();

	/* -p is accepted before the interface */
	ut_asserteq(1, run_commandf("sysboot -p host 0:0 ntfs %lx cfg",
				    (ulong)CONFIG_SYS_LOAD_ADDR));
	ut_assert_nextline("Invalid filesystem: ntfs");
	ut_assert_console_end();

	ut_assertok(env_set("bootfile", NULL));

	return 0;
}
CMD_TEST(cmd_test_sysboot_args, UTF_CONSOLE);
