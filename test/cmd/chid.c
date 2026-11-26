// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for chid command
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <chid.h>
#include <command.h>
#include <console.h>
#include <env.h>
#include <malloc.h>
#include <test/cmd.h>
#include <test/ut.h>
#include <version.h>

/*
 * The CHID of each variant on sandbox, where the SMBIOS values come from the
 * test devicetree. The first three take in the BIOS version, so their UUIDs
 * change with every build and only the label is checked.
 */
static const char *const sandbox_chid[CHID_VARIANT_COUNT] = {
	NULL,
	NULL,
	NULL,
	"a07db1d8-e0f9-5d47-a507-8c358eb7edf4",
	"5efd2af9-23d6-5fe6-bdb0-e040b9e5b054",
	"48aede6f-65db-51a5-8905-fdabdbc0685e",
	"d1c25f0a-6eb8-5c23-961c-e46694384fa0",
	"84398dad-5de1-553b-87b9-1b7c4b02c505",
	"ed1fa480-f06b-52ba-b203-56562c52c7e2",
	"c0185db1-6111-5432-955a-e5ecdac0d351",
	"6b7ad4ae-1f41-52dd-8499-ab9ab9e8688c",
	"72523836-a3fc-57b0-8d3c-4ce56cbf6b96",
	"e93b37fb-5592-55dd-8e4b-9df3fbc9abde",
	"d67e233f-5cad-541c-87f8-ee9bca569e5e",
	"45c5e2e7-db48-556b-aae4-0a03c5a15eae",
};

/* Test the 'chid show' sub-command */
static int cmd_test_chid_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("chid show", 0));

	ut_assert_nextline("Manufacturer:      Sandbox Corp");
	ut_assert_nextline("Family:            Sandbox_Family");
	ut_assert_nextline("Product Name:      Sandbox Computer");
	ut_assert_nextline("Product SKU:       SANDBOX-SKU");
	ut_assert_nextline("Baseboard Manuf:   Sandbox Boards");
	ut_assert_nextline("Baseboard Product: Sandbox Motherboard");
	ut_assert_nextline("BIOS Vendor:       Deinde Concept U-Boot");
	ut_assert_nextlinen("BIOS Version:      " PLAIN_VERSION);
	ut_assert_nextline("BIOS Major:        %u", U_BOOT_VERSION_NUM % 100);
	ut_assert_nextline("BIOS Minor:        %u", U_BOOT_VERSION_NUM_PATCH);
	ut_assert_nextline("Enclosure Type:    2");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_chid_base, UTF_CONSOLE);

/* Test the 'chid list' sub-command */
static int cmd_test_chid_list(struct unit_test_state *uts)
{
	int variant;

	ut_assertok(run_command("chid list", 0));

	for (variant = 0; variant < CHID_VARIANT_COUNT; variant++) {
		if (sandbox_chid[variant])
			ut_assert_nextline("HardwareID-%02d: %s", variant,
					   sandbox_chid[variant]);
		else
			ut_assert_nextlinen("HardwareID-%02d: ", variant);
	}
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_chid_list, UTF_CONSOLE);

/* Test the 'chid detail' sub-command */
static int cmd_test_chid_detail(struct unit_test_state *uts)
{
	/* the least specific variant uses the manufacturer alone */
	ut_assertok(run_command("chid detail 14", 0));
	ut_assert_nextline("HardwareID-14: %s", sandbox_chid[14]);
	ut_assert_nextline("Fields: Manufacturer");
	ut_assert_console_end();

	/* one in the middle, which leaves out the SKU and the baseboard */
	ut_assertok(run_command("chid detail 5", 0));
	ut_assert_nextline("HardwareID-05: %s", sandbox_chid[5]);
	ut_assert_nextline("Fields: Manufacturer + Family + ProductName");
	ut_assert_console_end();

	/* the most specific variant takes in eight fields */
	ut_assertok(run_command("chid detail 0", 0));
	ut_assert_nextlinen("HardwareID-00: ");
	ut_assert_nextline(
		"Fields: Manufacturer + Family + ProductName + ProductSku + "
		"BiosVendor + BiosVersion + BiosMajorRelease + "
		"BiosMinorRelease");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_chid_detail, UTF_CONSOLE);

/* Test the 'chid compat' sub-command */
static int cmd_test_chid_compat(struct unit_test_state *uts)
{
	const char *val;
	char *saved = NULL;

	/*
	 * The variable must come back as it was, since a later test or a boot
	 * may rely on it
	 */
	val = env_get("fdtcompat");
	if (val) {
		saved = strdup(val);
		ut_assertnonnull(saved);
	}
	ut_assertok(env_set("fdtcompat", NULL));

	/* sandbox has a /chid node whose hardware IDs are its own */
	ut_assertok(run_command("chid compat", 0));
	ut_assert_nextline("sandbox,test-device-1");
	ut_assert_console_end();
	ut_asserteq_str("sandbox,test-device-1", env_get("fdtcompat"));

	ut_assertok(env_set("fdtcompat", saved));
	free(saved);

	return 0;
}
CMD_TEST(cmd_test_chid_compat, UTF_CONSOLE);

/* Test what the command reports when it cannot do what it is asked */
static int cmd_test_chid_errors(struct unit_test_state *uts)
{
	/* the variant must be one which the specification defines */
	ut_asserteq(1, run_command("chid detail 15", 0));
	ut_assert_nextline("Invalid variant 15 (must be 0-14)");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_chid_errors, UTF_CONSOLE);

/**
 * expect_usage() - Run a command which shows the usage message
 *
 * @uts: Test state
 * @cmd: Command to run
 * Return: 0 if OK, 1 on failure
 */
static int expect_usage(struct unit_test_state *uts, const char *cmd)
{
	ut_asserteq(1, run_command(cmd, 0));
	ut_assert_nextline("chid - Computer Hardware ID utilities");

	/* the rest is the usage text, which this test does not check */
	console_record_reset();

	return 0;
}

/* Test the arguments which show the usage message */
static int cmd_test_chid_usage(struct unit_test_state *uts)
{
	/* a sub-command is required, and must be one of those on offer */
	ut_assertok(expect_usage(uts, "chid"));
	ut_assertok(expect_usage(uts, "chid other"));

	/* 'detail' wants a variant and nothing else */
	ut_assertok(expect_usage(uts, "chid detail"));
	ut_assertok(expect_usage(uts, "chid detail 0 1"));

	/* the other three take no arguments at all */
	ut_assertok(expect_usage(uts, "chid compat extra"));
	ut_assertok(expect_usage(uts, "chid list extra"));
	ut_assertok(expect_usage(uts, "chid show extra"));

	/* no sub-command takes an option, so each refuses one */
	ut_assertok(expect_usage(uts, "chid compat -x"));
	ut_assertok(expect_usage(uts, "chid list -x"));
	ut_assertok(expect_usage(uts, "chid show -x"));
	ut_assertok(expect_usage(uts, "chid detail -x"));

	/* a negative variant is an option as far as getopt() is concerned */
	ut_assertok(expect_usage(uts, "chid detail -1"));

	return 0;
}
CMD_TEST(cmd_test_chid_usage, UTF_CONSOLE);
