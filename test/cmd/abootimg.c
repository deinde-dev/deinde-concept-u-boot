// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the abootimg command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <android_image.h>
#include <command.h>
#include <env.h>
#include <image.h>
#include <mapmem.h>
#include <test/cmd.h>
#include <test/ut.h>

/*
 * Where the test builds its boot image, in the region the board keeps for
 * loading images
 */
#define IMAGE_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x100000)

/**
 * reset_addrs() - Put the image addresses back to their default
 *
 * The command holds the addresses in statics which outlive the test, where -1
 * means 'use $loadaddr'. That is the state at start-up and the one the
 * test.py tests expect, since a single U-Boot runs them all.
 */
static void reset_addrs(void)
{
	set_abootimg_addr(-1);
	set_avendor_bootimg_addr(-1);
}

/**
 * build_image() - Put a boot image header in memory and point the command at it
 *
 * Only the header is written, with the sizes of the kernel, the ramdisk and
 * the DTB area left at zero, which is enough for the fields the command reads
 * out of the header itself.
 *
 * @uts: Test state
 * @magic: Magic string to write, ANDR_BOOT_MAGIC for a valid header
 * @version: Header version to claim
 * Return: 0 if OK, other value on error
 */
static int build_image(struct unit_test_state *uts, const char *magic,
		       u32 version)
{
	struct andr_boot_img_hdr_v0 *hdr;

	hdr = map_sysmem(IMAGE_ADDR, sizeof(*hdr));
	memset(hdr, '\0', sizeof(*hdr));
	memcpy(hdr->magic, magic, strlen(magic));
	hdr->header_version = version;
	hdr->page_size = 4096;
	hdr->header_size = sizeof(*hdr);
	unmap_sysmem(hdr);

	ut_assertok(run_commandf("abootimg addr %lx", IMAGE_ADDR));
	ut_assert_console_end();

	return 0;
}

/**
 * check_usage() - Check the usage message the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("abootimg - manipulate Android Boot Image");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_skip_to_line(
			"      [size_var]: variable name to contain ramdisk size");
	}
	ut_assert_console_end();

	return 0;
}

/* Test reading the header version, both to the console and to a variable */
static int cmd_test_abootimg_base(struct unit_test_state *uts)
{
	ut_assertok(build_image(uts, ANDR_BOOT_MAGIC, 2));

	ut_assertok(run_command("abootimg get ver", 0));
	ut_assert_nextline("2");
	ut_assert_console_end();

	ut_assertok(run_command("abootimg get ver abootimg_ver", 0));
	ut_asserteq_str("2", env_get("abootimg_ver"));
	ut_assert_console_end();
	reset_addrs();

	return 0;
}
CMD_TEST(cmd_test_abootimg_base, UTF_CONSOLE);

/* Test that an image with no Android header is refused */
static int cmd_test_abootimg_nohdr(struct unit_test_state *uts)
{
	ut_assertok(build_image(uts, "DROID!!!", 2));

	ut_asserteq(1, run_command("abootimg get ver", 0));
	ut_assert_nextline("Error: Boot Image header is incorrect");
	ut_assert_console_end();

	/* the recovery DTBO area is read from the same header */
	ut_asserteq(1, run_command("abootimg get recovery_dtbo", 0));
	ut_assert_nextline("Error: Boot Image header is incorrect");
	ut_assert_console_end();
	reset_addrs();

	return 0;
}
CMD_TEST(cmd_test_abootimg_nohdr, UTF_CONSOLE);

/* Test the addresses the addr sub-command takes */
static int cmd_test_abootimg_addr(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("abootimg addr", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_commandf("abootimg addr %lx 0 0 0", IMAGE_ADDR));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("abootimg addr not-a-number", 0));
	ut_assert_nextline("Error: Wrong image address");
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("abootimg addr %lx junk", IMAGE_ADDR));
	ut_assert_nextline("Error: Wrong vendor_boot image address");
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("abootimg addr %lx 0 junk", IMAGE_ADDR));
	ut_assert_nextline("Error: Wrong init_boot image address");
	ut_assert_console_end();
	reset_addrs();

	return 0;
}
CMD_TEST(cmd_test_abootimg_addr, UTF_CONSOLE);

/* Test the index the dtb sub-command needs */
static int cmd_test_abootimg_dtb(struct unit_test_state *uts)
{
	ut_assertok(build_image(uts, ANDR_BOOT_MAGIC, 2));

	/* the index is the only way of naming a blob */
	ut_asserteq(1, run_command("abootimg get dtb", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("abootimg get dtb 0", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("abootimg get dtb --index=", 0));
	ut_assert_nextline("Error: Wrong index num");
	ut_assert_console_end();

	ut_asserteq(1, run_command("abootimg get dtb --index=1x", 0));
	ut_assert_nextline("Error: Wrong index num");
	ut_assert_console_end();
	reset_addrs();

	return 0;
}
CMD_TEST(cmd_test_abootimg_dtb, UTF_CONSOLE);

/* Test that the command refuses a sub-command it does not have */
static int cmd_test_abootimg_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("abootimg", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("abootimg frobnicate", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("abootimg get", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("abootimg get frobnicate", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("abootimg dump frobnicate", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_abootimg_usage, UTF_CONSOLE);
