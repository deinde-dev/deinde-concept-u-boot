// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the booti command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <env.h>
#include <errno.h>
#include <image.h>
#include <mapmem.h>
#include <asm/global_data.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Name of the function which logs the message sandbox gives instead of
 * booting, right-aligned in CONFIG_LOGF_FUNC_PAD (20) characters as the log
 * emits it
 */
#define SETUP_FUNC	"         booti_setup"

/* What sandbox says when it is asked to boot an Image */
#define NO_BOOT_MSG	"Booting is not supported on the sandbox."

/* Size of the Image, also used as the size of the compressed data */
#define IMAGE_SIZE	0x100

/*
 * Where the Image is built, in the region the board keeps for loading images
 */
#define IMAGE_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x100000)

/**
 * check_log() - Check the next console line, which the command logs
 *
 * The log format shows the function name only when CONFIG_LOGF_FUNC is
 * enabled. That is true of sandbox but of none of the other sandbox variants,
 * so the expected line differs between them.
 *
 * @uts: Test state
 * @msg: Message which is logged
 */
static int check_log(struct unit_test_state *uts, const char *msg)
{
	if (IS_ENABLED(CONFIG_LOGF_FUNC))
		ut_assert_nextline("%s() %s", SETUP_FUNC, msg);
	else
		ut_assert_nextline("%s", msg);

	return 0;
}

/* Test booting a flat Image, which is as far as sandbox can take it */
static int cmd_test_booti_base(struct unit_test_state *uts)
{
	u8 *buf, *dflt;

	buf = map_sysmem(IMAGE_ADDR, IMAGE_SIZE);
	memset(buf, '\0', IMAGE_SIZE);

	/*
	 * The data carries no compression magic, so the command takes it as a
	 * flat Image and passes it to booti_setup(), which sandbox does not
	 * implement
	 */
	ut_asserteq(1, run_commandf("booti %lx", IMAGE_ADDR));
	ut_assertok(check_log(uts, NO_BOOT_MSG));
	ut_assert_console_end();

	/*
	 * Without an address the command uses the default load address, so
	 * clear the magic there too; an earlier test may have left data behind
	 */
	dflt = map_sysmem(image_load_addr, 2);
	memset(dflt, '\0', 2);
	ut_asserteq(1, run_command("booti", 0));
	ut_assertok(check_log(uts, NO_BOOT_MSG));
	ut_assert_console_end();

	unmap_sysmem(dflt);
	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_booti_base, UTF_CONSOLE);

/* Test that the command refuses an option rather than taking it as an address */
static int cmd_test_booti_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("booti -x", 0));
	ut_assert_nextline("booti - boot Linux kernel 'Image' format from memory");
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("booti [addr [initrd[:size]] [fdt]]");
	}

	/* the remaining lines of the help text depend on the configuration */
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_booti_usage, UTF_CONSOLE);

/* Test what the command must be told before it can decompress an Image */
static int cmd_test_booti_comp(struct unit_test_state *uts)
{
	u8 *buf;

	buf = map_sysmem(IMAGE_ADDR, IMAGE_SIZE);
	memset(buf, '\0', IMAGE_SIZE);

	/* a gzip header: the magic, the deflate method and no flags */
	buf[0] = 0x1f;
	buf[1] = 0x8b;
	buf[2] = 0x08;

	/* the command has no way to know how much data to decompress */
	ut_asserteq(1, run_commandf("booti %lx", IMAGE_ADDR));
	ut_assert_nextline("kernel_comp_size is not provided!");
	ut_assert_console_end();

	/*
	 * Sandbox cannot reserve space for the Image, so the command falls
	 * back to decompressing it at kernel_comp_addr_r
	 */
	ut_assertok(env_set_hex("kernel_comp_size", IMAGE_SIZE));
	ut_asserteq(1, run_commandf("booti %lx", IMAGE_ADDR));
	ut_assert_nextline("kernel_comp_addr_r is not provided!");
	ut_assert_console_end();

	/* the Image is written to that area, so it must be in memory */
	ut_assertok(env_set_hex("kernel_comp_addr_r", gd->ram_top + 1));
	ut_asserteq(1, run_commandf("booti %lx", IMAGE_ADDR));
	ut_assert_nextline("kernel_comp_addr_r is outside of DRAM range!");
	ut_assert_console_end();

	/* leave the environment as the tests which follow expect it */
	ut_assertok(env_set("kernel_comp_size", NULL));
	ut_assertok(env_set("kernel_comp_addr_r", NULL));

	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_booti_comp, UTF_CONSOLE);
