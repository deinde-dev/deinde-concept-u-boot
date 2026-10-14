// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the coninfo command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <dm.h>
#include <iomux.h>
#include <stdio_dev.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Column the uclass name starts at, counting the '|-- ' which opens the line */
#define CONINFO_UCLASS_COL	27

/**
 * check_device() - Check the line reported for one stdio device
 *
 * The expectation is built from the device's own flags, so the test holds on
 * any board. The column the uclass name lands in is checked rather than the
 * padding the command works out, since the two are different ways of saying
 * the same thing; a device whose name is too long for the column cannot be
 * checked that way, so only the start of its line is
 *
 * @uts: Test state
 * @sdev: Device to check
 * Return: 0 if OK, -ve on error
 */
static int check_device(struct unit_test_state *uts, struct stdio_dev *sdev)
{
	char line[STDIO_NAME_LEN + 40];
	char flags[3];
	int len, pos;

	len = 0;
	if (sdev->flags & DEV_FLAGS_INPUT)
		flags[len++] = 'I';
	if (sdev->flags & DEV_FLAGS_OUTPUT)
		flags[len++] = 'O';
	flags[len] = '\0';

	pos = snprintf(line, sizeof(line), "|-- %s (%s)", sdev->name, flags);

	if (IS_ENABLED(CONFIG_CMD_CONSOLE_EXTRA) &&
	    IS_ENABLED(CONFIG_DM_STDIO) && (sdev->flags & DEV_FLAGS_DM)) {
		struct udevice *dev = sdev->priv;

		if (pos >= CONINFO_UCLASS_COL) {
			ut_assert_nextlinen(line);

			return 0;
		}
		snprintf(line + pos, sizeof(line) - pos, "%*s%s",
			 CONINFO_UCLASS_COL - pos, "",
			 dev_get_uclass_name(dev));
	}
	ut_assert_nextline("%s", line);

	return 0;
}

/**
 * check_listing() - Check the whole listing against the stdio devices
 *
 * @uts: Test state
 * Return: 0 if OK, -ve on error
 */
static int check_listing(struct unit_test_state *uts)
{
	struct list_head *list = stdio_get_list();
	struct list_head *pos;

	ut_assert_nextline("List of available devices");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_CMD_CONSOLE_EXTRA))
		ut_assert_nextline("Device  File               Uclass");

	list_for_each(pos, list) {
		struct stdio_dev *sdev;
		int l;

		sdev = list_entry(pos, struct stdio_dev, list);
		ut_assertok(check_device(uts, sdev));

		/* each file the device serves is listed under it, in order */
		for (l = 0; l < MAX_FILES; l++) {
			bool used;

			if (IS_ENABLED(CONFIG_CONSOLE_MUX))
				used = iomux_match_device(console_devices[l],
							  cd_count[l],
							  sdev) >= 0;
			else
				used = stdio_devices[l] == sdev;
			if (used)
				ut_assert_nextline("|   |-- %s",
						   stdio_names[l]);
		}
	}
	ut_assert_console_end();

	return 0;
}

/* Test 'coninfo' */
static int cmd_test_coninfo_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("coninfo", 0));
	ut_assertok(check_listing(uts));

	return 0;
}
CMD_TEST(cmd_test_coninfo_base, UTF_CONSOLE);

/* Test 'coninfo' with more arguments than it accepts */
static int cmd_test_coninfo_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("coninfo a b c", 0));
	ut_assert_nextline("coninfo - print console devices and information");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextlinen("coninfo");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_coninfo_usage, UTF_CONSOLE);
