// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for bootdev functions. All start with 'bootmeth'
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <bootmeth.h>
#include <bootstd.h>
#include <dm.h>
#include <env.h>
#include <test/ut.h>
#include "bootstd_common.h"

/* Lines of the usage message, each too long to write where it is used */
#define USAGE_LIST	\
	"bootmeth list [-a]     - list available bootmeths (-a all)"
#define USAGE_ORDER	\
	"bootmeth order [\"<bm> ...\"]  - select bootmeth order / subset to use"
#define USAGE_SET	\
	"bootmeth set <bootmeth> <property> <value> - set optional property"

/**
 * check_usage() - Check the usage message which the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("bootmeth - Boot methods");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline(USAGE_LIST);
	ut_assert_nextline(USAGE_ORDER);
	ut_assert_nextline(USAGE_SET);
	ut_assert_console_end();

	return 0;
}

/* Check 'bootmeth list' command */
static int bootmeth_cmd_list(struct unit_test_state *uts)
{
	ut_assertok(run_command("bootmeth list", 0));
	ut_assert_nextline("Order  Seq  Name                Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("    0    0  extlinux            Extlinux boot from a block device");
	ut_assert_nextline("    1    1  efi                 EFI boot from an .efi file");
	ut_assert_nextline("    2    2  vbe                 VBE A/B/recovery for OS");
	if (IS_ENABLED(CONFIG_BOOTMETH_GLOBAL))
		ut_assert_nextline(" glob    3  firmware0           VBE simple");
	ut_assert_nextlinen("---");
	ut_assert_nextline(IS_ENABLED(CONFIG_BOOTMETH_GLOBAL) ?
		 "(4 bootmeths)" : "(3 bootmeths)");
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(bootmeth_cmd_list, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Check 'bootmeth order' command */
static int bootmeth_cmd_order(struct unit_test_state *uts)
{
	/* Select just one bootmethod */
	ut_assertok(run_command("bootmeth order extlinux", 0));
	ut_assert_console_end();
	ut_assertnonnull(env_get("bootmeths"));
	ut_asserteq_str("extlinux", env_get("bootmeths"));

	/* Only that one should be listed */
	ut_assertok(run_command("bootmeth list", 0));
	ut_assert_nextline("Order  Seq  Name                Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("    0    0  extlinux            Extlinux boot from a block device");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(1 bootmeth)");
	ut_assert_console_end();

	/* Check the -a flag, efi should show as not in the order ("-") */
	ut_assertok(run_command("bootmeth list -a", 0));
	ut_assert_nextline("Order  Seq  Name                Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("    0    0  extlinux            Extlinux boot from a block device");
	ut_assert_nextline("    -    1  efi                 EFI boot from an .efi file");
	ut_assert_nextline("    -    2  vbe                 VBE A/B/recovery for OS");
	if (IS_ENABLED(CONFIG_BOOTMETH_GLOBAL))
		ut_assert_nextline(" glob    3  firmware0           VBE simple");
	ut_assert_nextlinen("---");
	ut_assert_nextline(IS_ENABLED(CONFIG_BOOTMETH_GLOBAL) ?
		 "(4 bootmeths)" : "(3 bootmeths)");
	ut_assert_console_end();

	/* Check the -a flag with the reverse order */
	ut_assertok(run_command("bootmeth order \"efi extlinux\"", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootmeth list -a", 0));
	ut_assert_nextline("Order  Seq  Name                Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("    1    0  extlinux            Extlinux boot from a block device");
	ut_assert_nextline("    0    1  efi                 EFI boot from an .efi file");
	ut_assert_nextline("    -    2  vbe                 VBE A/B/recovery for OS");
	if (IS_ENABLED(CONFIG_BOOTMETH_GLOBAL))
		ut_assert_nextline(" glob    3  firmware0           VBE simple");
	ut_assert_nextlinen("---");
	ut_assert_nextline(IS_ENABLED(CONFIG_BOOTMETH_GLOBAL) ?
		 "(4 bootmeths)" : "(3 bootmeths)");
	ut_assert_console_end();

	/* Now reset the order to empty, which should show all of them again */
	ut_assertok(run_command("bootmeth order", 0));
	ut_assert_console_end();
	ut_assertnull(env_get("bootmeths"));
	ut_assertok(run_command("bootmeth list", 0));
	ut_assert_skip_to_line(IS_ENABLED(CONFIG_BOOTMETH_GLOBAL) ?
		 "(4 bootmeths)" : "(3 bootmeths)");

	/* Try reverse order */
	ut_assertok(run_command("bootmeth order \"efi extlinux\"", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootmeth list", 0));
	ut_assert_nextline("Order  Seq  Name                Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("    0    1  efi                 EFI boot from an .efi file");
	ut_assert_nextline("    1    0  extlinux            Extlinux boot from a block device");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(2 bootmeths)");
	ut_assertnonnull(env_get("bootmeths"));
	ut_asserteq_str("efi extlinux", env_get("bootmeths"));
	ut_assert_console_end();

	/* A name which no bootmeth has leaves the order as it is */
	ut_asserteq(1, run_command("bootmeth order \"efi fred\"", 0));
	ut_assert_nextline("Unknown bootmeth 'fred'");
	ut_assert_nextline("Failed (err=-19)");
	ut_assert_console_end();
	ut_asserteq_str("efi extlinux", env_get("bootmeths"));
	ut_assertok(run_command("bootmeth list", 0));
	ut_assert_skip_to_line("(2 bootmeths)");
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(bootmeth_cmd_order, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Check 'bootmeth order' command with global bootmeths */
static int bootmeth_cmd_order_glob(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_BOOTMETH_GLOBAL))
		return -EAGAIN;

	ut_assertok(run_command("bootmeth order \"efi firmware0\"", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootmeth list", 0));
	ut_assert_nextline("Order  Seq  Name                Description");
	ut_assert_nextlinen("---");
	ut_assert_nextline("    0    1  efi                 EFI boot from an .efi file");
	ut_assert_nextline(" glob    3  firmware0           VBE simple");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(2 bootmeths)");
	ut_assertnonnull(env_get("bootmeths"));
	ut_asserteq_str("efi firmware0", env_get("bootmeths"));
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(bootmeth_cmd_order_glob, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Check 'bootmeth set' command */
static int bootmeth_cmd_set(struct unit_test_state *uts)
{
	/* Check we can enable extlinux fallback */
	console_record_reset_enable();
	ut_assertok(run_command("bootmeth set extlinux fallback 1", 0));
	ut_assert_console_end();

	/* Check we can disable extlinux fallback */
	console_record_reset_enable();
	ut_assertok(run_command("bootmeth set extlinux fallback 0", 0));
	ut_assert_console_end();

	/* Check extlinux fallback unexpected value */
	console_record_reset_enable();
	ut_asserteq(1, run_command("bootmeth set extlinux fallback fred", 0));
	ut_assert_nextline("Unexpected value 'fred'");
	ut_assert_nextline("Failed (err=-22)");
	ut_assert_console_end();

	/* Check that we need to provide right number of parameters */
	ut_asserteq(1, run_command("bootmeth set extlinux fallback", 0));
	ut_assert_nextline("Required parameters not provided");
	ut_assert_console_end();

	/* Check that we need to provide a valid bootmethod */
	ut_asserteq(1, run_command("bootmeth set fred fallback 0", 0));
	ut_assert_nextline("Unknown bootmeth 'fred'");
	ut_assert_nextline("Failed (err=-19)");
	ut_assert_console_end();

	/* Check that we need to provide a valid property */
	ut_asserteq(1, run_command("bootmeth set extlinux fred 0", 0));
	ut_assert_nextline("Invalid option");
	ut_assert_nextline("Failed (err=-22)");
	ut_assert_console_end();

	/* Check that we need to provide a bootmeth that supports properties */
	ut_asserteq(1, run_command("bootmeth set efi fallback 0", 0));
	ut_assert_nextline("set_property not found");
	ut_assert_nextline("Failed (err=-19)");
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(bootmeth_cmd_set, UTF_DM | UTF_SCAN_FDT);

/* Check the ways of reaching the usage message of the 'bootmeth' command */
static int bootmeth_cmd_usage(struct unit_test_state *uts)
{
	/* no sub-command at all */
	ut_asserteq(1, run_command("bootmeth", 0));
	ut_assertok(check_usage(uts));

	/* a sub-command which does not exist */
	ut_asserteq(1, run_command("bootmeth fred", 0));
	ut_assertok(check_usage(uts));

	/* one argument too many for the sub-command */
	ut_asserteq(1, run_command("bootmeth list -a extra", 0));
	ut_assertok(check_usage(uts));

	/* an option which the sub-command does not have */
	ut_asserteq(1, run_command("bootmeth list -x", 0));
	ut_assertok(check_usage(uts));

	/* order and set have no options at all */
	ut_asserteq(1, run_command("bootmeth order -x", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("bootmeth set -x", 0));
	ut_assertok(check_usage(uts));

	/* an argument which the sub-command has no use for */
	ut_asserteq(1, run_command("bootmeth list fred", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("bootmeth order extlinux efi", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("bootmeth set extlinux fallback 1 extra", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
BOOTSTD_TEST(bootmeth_cmd_usage, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Check 'bootmeths' env var */
static int bootmeth_env(struct unit_test_state *uts)
{
	struct bootstd_priv *std;

	ut_assertok(bootstd_get_priv(&std));

	/* Select just one bootmethod */
	ut_assertok(env_set("bootmeths", "extlinux"));
	ut_asserteq(1, std->bootmeth_count);

	/* Select an invalid bootmethod */
	ut_asserteq(1, run_command("setenv bootmeths fred", 0));
	ut_assert_nextline("Unknown bootmeth 'fred'");
	ut_assert_nextlinen("## Error inserting");
	ut_assert_console_end();

	ut_assertok(env_set("bootmeths", "efi extlinux"));
	ut_asserteq(2, std->bootmeth_count);
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(bootmeth_env, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Check the get_state_desc() method */
static int bootmeth_state(struct unit_test_state *uts)
{
	struct udevice *dev;
	char buf[50];

	ut_assertok(uclass_first_device_err(UCLASS_BOOTMETH, &dev));
	ut_assertnonnull(dev);

	ut_assertok(bootmeth_get_state_desc(dev, buf, sizeof(buf)));
	ut_asserteq_str("OK", buf);

	return 0;
}
BOOTSTD_TEST(bootmeth_state, UTF_DM | UTF_SCAN_FDT);
