// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the fatinfo command
 *
 * These are manual tests, since they need a filesystem image. The Python side
 * makes the images and passes their paths in; see test/py/tests/test_fatinfo.py
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <env.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

#define ARG_IMAGE	0	/* fs_image: path to the FAT image */
#define ARG_RAW		1	/* raw_image: path to an image with no FAT */

/**
 * bind_image() - Attach an image which the Python side made
 *
 * @uts: Test state
 * @arg: Argument holding the path to the image
 * Return: 0 if OK, other value on error
 */
static int bind_image(struct unit_test_state *uts, int arg)
{
	const char *image = ut_str(arg);

	ut_assertnonnull(image);
	ut_assertok(run_commandf("host bind 0 %s", image));
	console_record_reset();

	return 0;
}

/**
 * check_info() - Check the report for the image the Python side makes
 *
 * The image is 1MB, so it holds 2048 sectors of 512 bytes and mkfs.vfat
 * lays a FAT12 filesystem on it, with no volume label
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_info(struct unit_test_state *uts)
{
	ut_assert_nextline("Interface:  host");
	ut_assert_nextline("  Device 0: Vendor: U-Boot Rev: 1.0 Prod: hostfile");
	ut_assert_nextline("            Type: Hard Disk");
	ut_assert_nextline("            Capacity: 1.0 MB = 0.0 GB (2048 x 512)");
	ut_assert_nextline("Filesystem: FAT12 \"NO NAME    \"");
	ut_assert_console_end();

	return 0;
}

/* Test that fatinfo reports the device and the filesystem on it */
static int cmd_test_fatinfo_base_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts, ARG_IMAGE));

	ut_assertok(run_command("fatinfo host 0", 0));
	ut_assertok(check_info(uts));

	/* the image has no partition table, so the whole device is used */
	ut_assertok(run_command("fatinfo host 0:0", 0));
	ut_assertok(check_info(uts));

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatinfo_base_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR }, { "raw_image", UT_ARG_STR });

/* Test that the device comes from the bootdevice variable when left out */
static int cmd_test_fatinfo_default_norun(struct unit_test_state *uts)
{
	const char *orig = env_get("bootdevice");
	char saved[32];

	ut_assertok(bind_image(uts, ARG_IMAGE));

	saved[0] = '\0';
	if (orig)
		strlcpy(saved, orig, sizeof(saved));

	ut_assertok(env_set("bootdevice", "0"));
	ut_assertok(run_command("fatinfo host", 0));
	ut_assertok(check_info(uts));

	ut_assertok(env_set("bootdevice", saved[0] ? saved : NULL));

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatinfo_default_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR }, { "raw_image", UT_ARG_STR });

/* Test that a device without a FAT filesystem on it is reported */
static int cmd_test_fatinfo_notfat_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts, ARG_RAW));

	ut_asserteq(1, run_command("fatinfo host 0", 0));

	/* the message names the partition the command settled on */
	ut_assert_nextline_empty();
	ut_assert_nextline("** Unable to use host 0:0 for fatinfo **");
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatinfo_notfat_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR }, { "raw_image", UT_ARG_STR });

/* Test that fatinfo complains about a device which is not there */
static int cmd_test_fatinfo_baddev_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts, ARG_IMAGE));

	ut_asserteq(1, run_command("fatinfo host 1", 0));
	ut_assert_nextline("** Bad device specification host 1 **");
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatinfo_baddev_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR }, { "raw_image", UT_ARG_STR });

/* the last line of the usage message, which several tests wait for */
#define USAGE_LAST \
	"    - print information about filesystem from 'dev' on 'interface'"

/* Test that fatinfo shows its usage message when it cannot go on */
static int cmd_test_fatinfo_usage_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts, ARG_IMAGE));

	/* the interface is the one argument which has no default */
	ut_asserteq(1, run_command("fatinfo", 0));
	ut_assert_nextline("fatinfo - print information about filesystem");
	ut_assert_nextline("%s", "");
	ut_assert_nextline("Usage:");
	ut_assert_nextline("fatinfo <interface> [<dev[:part]>]");
	ut_assert_nextline(USAGE_LAST);
	ut_assert_console_end();

	/* and there is nothing to give beyond the device */
	ut_asserteq(1, run_command("fatinfo host 0 extra", 0));
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatinfo_usage_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR }, { "raw_image", UT_ARG_STR });
