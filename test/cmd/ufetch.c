// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the ufetch command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <blk.h>
#include <command.h>
#include <console.h>
#include <dm.h>
#include <version.h>
#include <dm/ofnode.h>
#include <dm/uclass-internal.h>
#include <linux/string.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

/* Escape sequences the report is coloured with, as cmd/ufetch.c has them */
#define BLUE	"\033[34m"
#define BOLD	"\033[1m"
#define RESET	"\033[0m"

/* Height of the logo, which is drawn unless -n is given */
#define LOGO_LINES	25

/* Storage lines the report has room for */
#define STORAGE_LINES	10

/* Top row of the logo, which the compatible string sits beside */
#define LOGO_TOP	"                  ......::......"

/* Features sandbox builds in: NET, EFI_LOADER and CMD_CAT */
#define FEATURES	"Net, EFI, cat :3"

/**
 * check_storage() - Check the line for each block device
 *
 * Each line gives the uclass of the device and its device number, which is
 * not the same as its LUN. The size depends on whether an image is attached,
 * which the test harness decides, so only the start of the line is checked.
 *
 * Return: 0 if OK, other value on error
 */
static int check_storage(struct unit_test_state *uts)
{
	int i;

	for (i = 0; i < STORAGE_LINES; i++) {
		struct blk_desc *desc;
		struct udevice *dev;

		if (uclass_find_device_by_seq(UCLASS_BLK, i, &dev))
			break;
		desc = dev_get_uclass_plat(dev);
		ut_assert_nextlinen("%4s %d: " RESET,
				    blk_get_uclass_name(desc->uclass_id),
				    desc->devnum);
	}

	/* sandbox has three mmc devices, so the report cannot be empty */
	ut_assert(i > 0);

	return 0;
}

/* Check the report which -n prints, without the logo in the way */
static int cmd_test_ufetch_base(struct unit_test_state *uts)
{
	const char *compatible, *model;
	char dashes[80];
	int n_cpus = 0;
	ofnode np;

	compatible = ofnode_read_string(ofnode_root(), "compatible");
	if (!compatible)
		compatible = "unknown";
	ut_assert(strlen(compatible) < sizeof(dashes));
	memset(dashes, '-', strlen(compatible));
	dashes[strlen(compatible)] = '\0';

	ofnode_for_each_subnode(np, ofnode_path("/cpus")) {
		if (ofnode_name_eq(np, "cpu"))
			n_cpus++;
	}

	ut_assertok(run_command("ufetch -n", 0));

	ut_assert_nextline(RESET "%s", compatible);
	ut_assert_nextline("%s", dashes);
	ut_assert_nextline("Kernel:" RESET " %s", U_BOOT_VERSION);
	/* the line is there on any board with a config name, as sandbox has */
	ut_assert_nextline("Config:" RESET " %s_defconfig",
			   CONFIG_SYS_CONFIG_NAME);
	model = ofnode_read_string(ofnode_root(), "model");
	if (model)
		ut_assert_nextline("Host:" RESET " %s", model);

	/* the uptime, the address and the console list all vary by run */
	ut_assert_nextlinen("Uptime:" RESET " ");
	ut_assert_nextlinen("IP Address:" RESET " ");
	ut_assert_nextline("Commands:" RESET " %d (help)",
			   ll_entry_count(struct cmd_tbl, cmd));
	ut_assert_nextlinen("Consoles:" RESET " ");
	ut_assert_nextline("Features:" RESET " " FEATURES);
	ut_assert_nextlinen("Relocated:" RESET " ");
	ut_assert_nextline("CPU: " RESET CONFIG_SYS_ARCH " (%d cores, 1 in use)",
			   n_cpus);
	ut_assert_nextline("Memory:" RESET " %d MiB",
			   CONFIG_SANDBOX_RAM_SIZE_MB);
	ut_assertok(check_storage(uts));
	ut_assert_nextline(RESET);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_ufetch_base, UTF_CONSOLE);

/* Check that the logo is drawn when -n is left out */
static int cmd_test_ufetch_logo(struct unit_test_state *uts)
{
	int i;

	ut_assertok(run_command("ufetch", 0));

	/*
	 * Every line starts with the logo, whether or not a field is left to
	 * show beside it; the fields themselves are the base test's business
	 */
	ut_assert_nextlinen(BLUE BOLD LOGO_TOP);
	for (i = 1; i < LOGO_LINES; i++)
		ut_assert_nextlinen(BLUE BOLD);
	ut_assert_nextline(RESET);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_ufetch_logo, UTF_CONSOLE);

/**
 * check_usage() - Check the usage message
 *
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("ufetch - U-Boot fetch utility");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("ufetch Print information about your device.");
	ut_assert_nextline("    -n    Don't print the ASCII logo");
	ut_assert_console_end();

	return 0;
}

/* Check that a second argument is refused */
static int cmd_test_ufetch_extra(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("ufetch -n extra", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_ufetch_extra, UTF_CONSOLE);

/* Check that an option the command does not have is refused */
static int cmd_test_ufetch_badopt(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("ufetch -x", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_ufetch_badopt, UTF_CONSOLE);
