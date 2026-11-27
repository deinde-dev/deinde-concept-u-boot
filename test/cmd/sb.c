// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for sb command
 *
 * Copyright (C) 2025 Canonical Ltd
 */

#include <dm.h>
#include <asm/state.h>
#include <dm/test.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

#define MAP_HEADING	"    Addr           Mapping  Refcnt"

#define USAGE_FIRST_LINE	"sb - Sandbox status commands"
#define USAGE_DEVOFF	\
	"sb devoff <node>  - Disable device from device tree node"
#define USAGE_DEVON	\
	"sb devon <node>   - Enable device from device tree node"
#define USAGE_GRID	\
	"sb grid <0|1> [<size>] - Enable/disable grid overlay on video"
#define USAGE_HANDOFF	\
	"sb handoff        - Show handoff data received from SPL"
#define USAGE_MAP	"sb map            - Show mapped memory"
#define USAGE_STATE	"sb state          - Show sandbox state"

/* Check the usage message, which every bad command line shows */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline(USAGE_FIRST_LINE);
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline(USAGE_DEVOFF);
	ut_assert_nextline(USAGE_DEVON);
	ut_assert_nextline(USAGE_GRID);
	ut_assert_nextline(USAGE_HANDOFF);
	ut_assert_nextline(USAGE_MAP);
	ut_assert_nextline(USAGE_STATE);
	ut_assert_console_end();

	return 0;
}

/* Test 'sb map' and 'sb state' */
static int cmd_test_sb_base(struct unit_test_state *uts)
{
	struct sandbox_state *state = state_get_current();

	ut_assertok(run_command("sb map", 0));
	ut_assert_nextline("Sandbox memory-mapping");
	ut_assert_nextline(MAP_HEADING);

	/*
	 * Which mappings exist depends on what the sandbox process has asked
	 * map_sysmem() for so far, so there is nothing stable to check
	 */
	console_record_reset();

	ut_assertok(run_command("sb state", 0));
	ut_assert_nextline("Arguments:");
	ut_assert_nextlinen("%s", state->argv[0]);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_sb_base, UTF_CONSOLE);

/* Test 'sb handoff' */
static int cmd_test_sb_handoff(struct unit_test_state *uts)
{
	if (IS_ENABLED(CONFIG_HANDOFF)) {
		/*
		 * The message says whether the handoff arrived, which depends
		 * on whether SPL ran, so check only as far as both agree
		 */
		ut_assertok(run_command("sb handoff", 0));
		ut_assert_nextlinen("SPL handoff");
		ut_assert_console_end();
	} else {
		ut_asserteq(1, run_command("sb handoff", 0));
		ut_assert_nextline("Command not supported");
		ut_assertok(check_usage(uts));
	}

	return 0;
}
CMD_TEST(cmd_test_sb_handoff, UTF_CONSOLE);

/* Test the usage message */
static int cmd_test_sb_usage(struct unit_test_state *uts)
{
	/* a missing sub-command */
	ut_asserteq(1, run_command("sb", 0));
	ut_assertok(check_usage(uts));

	/* a sub-command which does not exist */
	ut_asserteq(1, run_command("sb fred", 0));
	ut_assertok(check_usage(uts));

	/* 'devon' and 'devoff' want a node name */
	ut_asserteq(1, run_command("sb devon", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("sb devoff", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("sb devon mmc11 extra", 0));
	ut_assertok(check_usage(uts));

	/* 'grid' wants a value */
	ut_asserteq(1, run_command("sb grid", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("sb grid 1 20 extra", 0));
	ut_assertok(check_usage(uts));

	/* 'handoff', 'map' and 'state' take no argument */
	ut_asserteq(1, run_command("sb handoff extra", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("sb map extra", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("sb state extra", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_sb_usage, UTF_CONSOLE);

/* Test that the sub-commands refuse an option */
static int cmd_test_sb_opt(struct unit_test_state *uts)
{
	/*
	 * 'devon' and 'devoff' read an option as the node name, so this used
	 * to report a missing node, and 'grid' read it as a value of zero
	 */
	ut_asserteq(1, run_command("sb devon -x", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("sb devoff -x", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("sb grid -x", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_sb_opt, UTF_CONSOLE);

/* Basic test of 'sb devon' and 'sb devoff' commands */
static int dm_test_sb_devon_devoff(struct unit_test_state *uts)
{
	struct udevice *dev;
	ofnode root, node;

	/* Find the mmc11 device tree node */
	root = oftree_root(oftree_default());
	node = ofnode_find_subnode(root, "mmc11");
	ut_assert(ofnode_valid(node));

	/* Verify device is not initially bound */
	ut_assert(device_find_global_by_ofnode(node, &dev));

	/* Enable the device using 'sb devon' */
	ut_assertok(run_command("sb devon mmc11", 0));
	ut_assert_nextline("Device 'mmc11' enabled");
	ut_assert_console_end();

	/* Verify device is now bound and probed */
	ut_assertok(device_find_global_by_ofnode(node, &dev));
	ut_assertnonnull(dev);
	ut_assert(device_active(dev));

	/* Disable the device using 'sb devoff' */
	ut_assertok(run_command("sb devoff mmc11", 0));
	ut_assert_nextline("Device 'mmc11' disabled");
	ut_assert_console_end();

	/* Verify device is no longer bound */
	ut_assert(device_find_global_by_ofnode(node, &dev));

	return 0;
}
DM_TEST(dm_test_sb_devon_devoff, UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'sb devon' with invalid node */
static int dm_test_sb_devon_invalid(struct unit_test_state *uts)
{
	/* Try to enable non-existent device */
	ut_asserteq(1, run_command("sb devon nonexistent", 0));
	ut_assert_nextline("Device tree node 'nonexistent' not found");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_sb_devon_invalid, UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'sb devoff' with invalid node */
static int dm_test_sb_devoff_invalid(struct unit_test_state *uts)
{
	/* Try to disable non-existent device */
	ut_asserteq(1, run_command("sb devoff nonexistent", 0));
	ut_assert_nextline("Device tree node 'nonexistent' not found");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_sb_devoff_invalid, UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'sb devon' on device that's already enabled */
static int dm_test_sb_devon_already_enabled(struct unit_test_state *uts)
{
	/* Enable the device first */
	ut_assertok(run_command("sb devon mmc11", 0));
	ut_assert_nextline("Device 'mmc11' enabled");
	ut_assert_console_end();

	/* Try to enable it again - should fail */
	ut_asserteq(1, run_command("sb devon mmc11", 0));
	ut_assert_nextline("Device 'mmc11' is already enabled");
	ut_assert_console_end();

	/* Clean up - disable the device */
	ut_assertok(run_command("sb devoff mmc11", 0));
	ut_assert_nextline("Device 'mmc11' disabled");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_sb_devon_already_enabled, UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'sb devoff' on device that's not bound */
static int dm_test_sb_devoff_not_bound(struct unit_test_state *uts)
{
	struct udevice *dev;
	ofnode root, node;

	/* Find the mmc11 device tree node */
	root = oftree_root(oftree_default());
	node = ofnode_find_subnode(root, "mmc11");
	ut_assert(ofnode_valid(node));

	/* Ensure device is not bound (clean up from any previous test) */
	if (!device_find_global_by_ofnode(node, &dev)) {
		ut_assertok(run_command("sb devoff mmc11", 0));
		ut_assert_nextlinen("Device 'mmc11' disabled");
		ut_assert_console_end();
	}

	/* Verify device is not bound */
	ut_assert(device_find_global_by_ofnode(node, &dev));

	/* Try to disable a device that's not bound */
	ut_asserteq(1, run_command("sb devoff mmc11", 0));
	ut_assert_nextlinen("Device 'mmc11' not found or not bound");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_sb_devoff_not_bound, UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'sb grid' command */
static int dm_test_sb_grid(struct unit_test_state *uts)
{
	struct sandbox_state *state = state_get_current();

	/* Ensure grid is initially off */
	state->show_grid = false;
	state->grid_size = 0;

	/* Enable grid */
	ut_assertok(run_command("sb grid 1", 0));
	ut_assert_console_end();
	ut_asserteq(true, state->show_grid);

	/* Disable grid */
	ut_assertok(run_command("sb grid 0", 0));
	ut_assert_console_end();
	ut_asserteq(false, state->show_grid);

	/* Enable grid with custom size (0x14 = 20 decimal) */
	ut_assertok(run_command("sb grid 1 14", 0));
	ut_assert_console_end();
	ut_asserteq(true, state->show_grid);
	ut_asserteq(0x14, state->grid_size);

	/* Clean up */
	state->show_grid = false;
	state->grid_size = 0;

	return 0;
}
DM_TEST(dm_test_sb_grid, UTF_CONSOLE);
