// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the pxe command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <env.h>
#include <mapmem.h>
#include <asm/global_data.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/* Address for the configuration file, clear of the one the loads tests use */
#define PXE_CFG_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x8000)

/*
 * A configuration file whose only label names no kernel. It has no 'menu'
 * keyword, so the label is selected without showing a menu
 */
static const char cfg_no_kernel[] =
	"default test\n"
	"label test\n"
	"\tappend root=/dev/sda1\n";

/**
 * write_cfg() - Put a configuration file in memory for 'pxe boot' to read
 *
 * @cfg: Nul-terminated file contents
 */
static void write_cfg(const char *cfg)
{
	uint len = strlen(cfg) + 1;
	char *ptr;

	ptr = map_sysmem(PXE_CFG_ADDR, len);
	memcpy(ptr, cfg, len);
	unmap_sysmem(ptr);
}

/* Check the usage message for a missing or unknown sub-command */
static int cmd_test_pxe_base(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("pxe", 0));
	ut_assert_nextline("pxe - get and boot from pxe files");
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextlinen("pxe get ");
		ut_assert_nextlinen("pxe boot ");
		ut_assert_nextline_empty();
	}
	ut_assert_console_end();

	/* an unknown sub-command shows the same message */
	ut_asserteq(1, run_command("pxe nosuch", 0));
	ut_assert_nextline("pxe - get and boot from pxe files");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_pxe_base, UTF_CONSOLE);

/* Check the argument checking of 'pxe get' */
static int cmd_test_pxe_get(struct unit_test_state *uts)
{
	ut_assertok(env_set("pxefile_addr_r", NULL));

	/* with no address to load to, the command gives up at once */
	ut_asserteq(1, run_command("pxe get", 0));
	ut_assert_nextline("missing environment variable: pxefile_addr_r");
	ut_assert_console_end();

	/* -ipv6 is accepted, but only as the last parameter */
	if (IS_ENABLED(CONFIG_IPV6)) {
		ut_asserteq(1, run_command("pxe get -ipv6", 0));
		ut_assert_nextline(
			"missing environment variable: pxefile_addr_r");
		ut_assert_console_end();
	}

	/* anything else is a usage error */
	ut_asserteq(1, run_command("pxe get extra", 0));
	ut_assert_nextline("pxe - get and boot from pxe files");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_pxe_get, UTF_CONSOLE);

/* Check that 'pxe boot' parses the file at the address it is given */
static int cmd_test_pxe_boot(struct unit_test_state *uts)
{
	/* the address must be in memory on this board */
	if (gd->ram_base > PXE_CFG_ADDR)
		return -EAGAIN;

	ut_assertok(env_set("pxefile_addr_r", NULL));

	/* with no address at all the command gives up at once */
	ut_asserteq(1, run_command("pxe boot", 0));
	ut_assert_nextline("missing environment variable: pxefile_addr_r");
	ut_assert_console_end();

	/* an address which is not a hex number is rejected */
	ut_asserteq(1, run_command("pxe boot zzz", 0));
	ut_assert_nextline("Invalid pxefile address: zzz");
	ut_assert_console_end();

	/* two addresses are a usage error */
	ut_asserteq(1, run_commandf("pxe boot %lx %lx", (ulong)PXE_CFG_ADDR,
				    (ulong)PXE_CFG_ADDR));
	ut_assert_nextline("pxe - get and boot from pxe files");
	console_record_reset();

	/*
	 * A label with no kernel is skipped, which shows the file really was
	 * parsed. The command still succeeds, since parsing the file is all
	 * it undertakes to do.
	 */
	write_cfg(cfg_no_kernel);
	ut_assertok(run_commandf("pxe boot %lx", (ulong)PXE_CFG_ADDR));
	ut_assert_nextline("1:\ttest");
	ut_assert_nextline("No kernel given, skipping test");
	ut_assert_console_end();

	/* the address can come from the environment instead */
	ut_assertok(env_set_hex("pxefile_addr_r", PXE_CFG_ADDR));
	ut_assertok(run_command("pxe boot", 0));
	ut_assert_nextline("1:\ttest");
	ut_assert_nextline("No kernel given, skipping test");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_pxe_boot, UTF_CONSOLE);
