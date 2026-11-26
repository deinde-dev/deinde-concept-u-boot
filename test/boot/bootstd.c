// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the bootstd command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <test/ut.h>
#include "bootstd_common.h"

/* The two lines of the image table which have no data in them */
#define HEADER	\
	"Seq  Bootflow             Type                  At      Size  Filename"
#define SEP	\
	"---  -------------------  --------------  --------  --------  ----------------"

/**
 * check_usage() - Check the usage message which the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("bootstd - Standard-boot operation");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("bootstd images      - list loaded images");
	ut_assert_console_end();

	return 0;
}

/* Check the ways of reaching the usage message */
static int bootstd_cmd_usage(struct unit_test_state *uts)
{
	/* no sub-command at all */
	ut_asserteq(1, run_command("bootstd", 0));
	ut_assertok(check_usage(uts));

	/* a sub-command which does not exist */
	ut_asserteq(1, run_command("bootstd fred", 0));
	ut_assertok(check_usage(uts));

	/* an argument which the sub-command has no use for */
	ut_asserteq(1, run_command("bootstd images extra", 0));
	ut_assertok(check_usage(uts));

	/* the sub-command has no options */
	ut_asserteq(1, run_command("bootstd images -x", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
BOOTSTD_TEST(bootstd_cmd_usage, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Check 'bootstd images' before anything is scanned */
static int bootstd_cmd_images_empty(struct unit_test_state *uts)
{
	ut_assertok(run_command("bootstd images", 0));
	ut_assert_nextline(HEADER);
	ut_assert_nextline(SEP);
	ut_assert_nextline(SEP);
	ut_assert_nextline("(0 images)");
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(bootstd_cmd_images_empty, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);
