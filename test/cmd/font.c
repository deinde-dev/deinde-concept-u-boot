// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for font command
 *
 * Copyright 2022 Google LLC
 */

#include <console.h>
#include <dm.h>
#include <video_console.h>
#include <test/ut.h>

/* Declare a new font test */
#define FONT_TEST(_name, _flags)	UNIT_TEST(_name, _flags, font)

/* Test the 'font' command */
static int font_test_base(struct unit_test_state *uts)
{
	struct udevice *dev;
	const char *name;
	int max_metrics;
	uint size;
	int ret;

	ut_assertok(uclass_first_device_err(UCLASS_VIDEO, &dev));
	ut_assertok(uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &dev));

	ut_assertok(run_command("font list", 0));
	if (IS_ENABLED(CONFIG_VIDEO_FONT_8X16))
		ut_assert_nextline("8x16");
	if (IS_ENABLED(CONFIG_VIDEO_FONT_SUN12X22))
		ut_assert_nextline("12x22");
	if (IS_ENABLED(CONFIG_CONSOLE_TRUETYPE_NIMBUS))
		ut_assert_nextline("nimbus_sans_l_regular");
	if (IS_ENABLED(CONFIG_CONSOLE_TRUETYPE_ANKACODER))
		ut_assert_nextline("ankacoder_c75_r");
	if (IS_ENABLED(CONFIG_CONSOLE_TRUETYPE_CANTORAONE))
		ut_assert_nextline("cantoraone_regular");
	if (IS_ENABLED(CONFIG_CONSOLE_TRUETYPE_UBUNTU_LIGHT))
		ut_assert_nextline("ubuntu_light");
	if (IS_ENABLED(CONFIG_CONSOLE_TRUETYPE_UBUNTU_BOLD))
		ut_assert_nextline("ubuntu_bold");

	ut_assert_console_end();

	ut_assertok(vidconsole_get_font_size(dev, NULL, &name, &size));
	if (IS_ENABLED(CONFIG_CONSOLE_TRUETYPE_ANKACODER))
		ut_asserteq_str("ankacoder_c75_r", name);
	else if (IS_ENABLED(CONFIG_CONSOLE_TRUETYPE_NIMBUS))
		ut_asserteq_str("nimbus_sans_l_regular", name);
	else
		ut_asserteq_str("8x16", name);
	ut_asserteq(CONFIG_CONSOLE_TRUETYPE_SIZE, size);

	if (!IS_ENABLED(CONFIG_CONSOLE_TRUETYPE_CANTORAONE))
		return 0;

	max_metrics = 1;
	if (IS_ENABLED(CONFIG_CONSOLE_TRUETYPE))
		max_metrics = IF_ENABLED_INT(CONFIG_CONSOLE_TRUETYPE,
				     CONFIG_CONSOLE_TRUETYPE_MAX_METRICS);

	ret = run_command("font select cantoraone_regular 40", 0);
	if (max_metrics < 2) {
		ut_asserteq(1, ret);
		ut_assert_nextline("Failed (error -7)");
		ut_assert_console_end();
		return 0;
	}

	ut_assertok(ret);
	ut_assert_console_end();

	ut_assertok(vidconsole_get_font_size(dev, NULL, &name, &size));
	ut_asserteq_str("cantoraone_regular", name);
	ut_asserteq(40, size);
	ut_assertok(ut_check_console_end(uts));

	ut_assertok(run_command("font size", 0));
	ut_assert_nextline("40");
	ut_assertok(ut_check_console_end(uts));

	ut_assertok(run_command("font size 30", 0));
	ut_assert_console_end();

	ut_assertok(run_command("font size", 0));
	ut_assert_nextline("30");
	ut_assertok(ut_check_console_end(uts));

	ut_assertok(run_command("font select", 0));
	ut_assertok(ut_check_console_end(uts));

	ut_assertok(vidconsole_get_font_size(dev, NULL, &name, &size));
	ut_asserteq_str("nimbus_sans_l_regular", name);
	ut_asserteq(CONFIG_CONSOLE_TRUETYPE_SIZE, size);

	return 0;
}
FONT_TEST(font_test_base, UTF_SCAN_PDATA | UTF_SCAN_FDT | UTF_CONSOLE |
	  UTF_DM);

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
	ut_assert_nextline("font - Fonts");

	/* the rest is the usage text, which this test does not check */
	console_record_reset();

	return 0;
}

/* Test 'font select' with a name the console does not have */
static int font_test_select(struct unit_test_state *uts)
{
	struct udevice *dev;
	const char *name;
	uint size;

	ut_assertok(uclass_first_device_err(UCLASS_VIDEO, &dev));
	ut_assertok(uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &dev));

	/* the console reports -ENOENT, which the command shows */
	ut_asserteq(1, run_command("font select not-a-font", 0));
	ut_assert_nextline("Failed (error -2)");
	ut_assert_console_end();

	if (!IS_ENABLED(CONFIG_VIDEO_FONT_8X16))
		return 0;

	/*
	 * a bitmap font keeps its own size, whatever is asked for, and the
	 * TrueType console reports the height of its characters
	 */
	ut_assertok(run_command("font select 8x16 20", 0));
	ut_assert_console_end();

	ut_assertok(vidconsole_get_font_size(dev, NULL, &name, &size));
	ut_asserteq_str("8x16", name);
	ut_asserteq(16, size);

	return 0;
}
FONT_TEST(font_test_select, UTF_SCAN_PDATA | UTF_SCAN_FDT | UTF_CONSOLE |
	  UTF_DM);

/* Test 'font size' */
static int font_test_size(struct unit_test_state *uts)
{
	struct udevice *dev;
	const char *name;
	uint size;

	/*
	 * this file is built only with CONFIG_CONSOLE_TRUETYPE, so the size
	 * and metrics options are set; a second size needs a second set of
	 * metrics to hold it
	 */
	if (CONFIG_CONSOLE_TRUETYPE_MAX_METRICS < 2)
		return -EAGAIN;

	ut_assertok(uclass_first_device_err(UCLASS_VIDEO, &dev));
	ut_assertok(uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &dev));

	/* the console starts at the size set by the Kconfig */
	ut_assertok(run_command("font size", 0));
	ut_assert_nextline("%d", CONFIG_CONSOLE_TRUETYPE_SIZE);
	ut_assert_console_end();

	/* a new size is quietly accepted and shown by the next 'font size' */
	ut_assertok(run_command("font size 30", 0));
	ut_assert_console_end();

	ut_assertok(run_command("font size", 0));
	ut_assert_nextline("30");
	ut_assert_console_end();

	/* the font itself does not change */
	ut_assertok(vidconsole_get_font_size(dev, NULL, &name, &size));
	ut_asserteq(30, size);

	/* a size which is not a number reads as 0, which is the default */
	ut_assertok(run_command("font size abc", 0));
	ut_assert_console_end();

	ut_assertok(run_command("font size", 0));
	ut_assert_nextline("%d", CONFIG_CONSOLE_TRUETYPE_SIZE);
	ut_assert_console_end();

	return 0;
}
FONT_TEST(font_test_size, UTF_SCAN_PDATA | UTF_SCAN_FDT | UTF_CONSOLE |
	  UTF_DM);

/* Test the arguments which show the usage message */
static int font_test_usage(struct unit_test_state *uts)
{
	/* a sub-command is required, and must be one of those on offer */
	ut_assertok(expect_usage(uts, "font"));
	ut_assertok(expect_usage(uts, "font other"));

	/* 'list' takes no arguments */
	ut_assertok(expect_usage(uts, "font list extra"));

	/* 'select' takes a name and a size, 'size' just a size */
	ut_assertok(expect_usage(uts, "font select 8x16 20 extra"));
	ut_assertok(expect_usage(uts, "font size 20 extra"));

	if (IS_ENABLED(CONFIG_VIDEO_GLYPH_STATS))
		ut_assertok(expect_usage(uts, "font info extra"));

	/* no sub-command takes an option, refused only with CMD_ARG_CHECK */
	if (IS_ENABLED(CONFIG_CMD_ARG_CHECK)) {
		ut_assertok(expect_usage(uts, "font list -x"));
		ut_assertok(expect_usage(uts, "font select -x"));
		ut_assertok(expect_usage(uts, "font size -x"));
		if (IS_ENABLED(CONFIG_VIDEO_GLYPH_STATS))
			ut_assertok(expect_usage(uts, "font info -x"));
	}

	return 0;
}
FONT_TEST(font_test_usage, UTF_CONSOLE);

/* Test 'font info' command */
static int font_test_info(struct unit_test_state *uts)
{
	int count;

	if (!IS_ENABLED(CONFIG_VIDEO_GLYPH_STATS))
		return -EAGAIN;

	count = gd_glyph_count();
	ut_assertok(run_command("font info", 0));
	ut_assert_nextline("glyphs rendered: %u", count);
	ut_assert_console_end();

	return 0;
}
FONT_TEST(font_test_info, UTF_CONSOLE);
