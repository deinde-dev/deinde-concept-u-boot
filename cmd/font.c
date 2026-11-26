// SPDX-License-Identifier: GPL-2.0+
/*
 * video commands
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <dm.h>
#include <getopt.h>
#include <video.h>
#include <video_console.h>

#if IS_ENABLED(CONFIG_VIDEO_GLYPH_STATS)
static int do_font_info(struct getopt_state *gs)
{
	printf("glyphs rendered: %u\n", gd->glyph_count);

	return 0;
}
#endif

static int do_font_list(struct getopt_state *gs)
{
	struct udevice *dev;

	if (uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &dev))
		return CMD_RET_FAILURE;
	vidconsole_list_fonts(dev);

	return 0;
}

static int do_font_select(struct getopt_state *gs)
{
	char *const *argv = gs->argv;
	int argc = gs->argc;
	struct udevice *dev;
	const char *name;
	uint size = 0;
	int ret;

	if (uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &dev))
		return CMD_RET_FAILURE;
	name = argv[1];
	if (argc == 3)
		size = dectoul(argv[2], NULL);
	ret = vidconsole_select_font(dev, NULL, name, size);
	if (ret)
		return cmd_fail(ret, "Failed (error %d)\n", ret);

	return 0;
}

static int do_font_size(struct getopt_state *gs)
{
	char *const *argv = gs->argv;
	int argc = gs->argc;
	const char *font_name;
	struct udevice *dev;
	uint size;
	int ret;

	if (uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &dev))
		return CMD_RET_FAILURE;
	ret = vidconsole_get_font_size(dev, NULL, &font_name, &size);
	if (ret)
		return cmd_fail(ret, "Failed (error %d)\n", ret);

	if (argc < 2) {
		printf("%d\n", size);
	} else {
		size = dectoul(argv[1], NULL);

		ret = vidconsole_select_font(dev, NULL, font_name, size);
		if (ret)
			return cmd_fail(ret, "Failed (error %d)\n", ret);
	}

	return 0;
}

#if IS_ENABLED(CONFIG_VIDEO_GLYPH_STATS)
#define FONT_INFO_HELP	"\nfont info - show glyph rendering statistics"
#else
#define FONT_INFO_HELP
#endif

U_BOOT_LONGHELP(font,
	"list       - list available fonts\n"
	"font select [<name> [<size>]] - select font to use\n"
	"font size [<size>] - show or set font size"
	FONT_INFO_HELP);

U_BOOT_CMD_WITH_SUBCMDS(font, "Fonts", font_help_text,
	U_BOOT_CMD_MKENT_NOOPTS(list, 1, 1, do_font_list, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(select, 3, 1, do_font_select, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(size, 2, 1, do_font_size, "", ""),
#if IS_ENABLED(CONFIG_VIDEO_GLYPH_STATS)
	U_BOOT_CMD_MKENT_NOOPTS(info, 1, 1, do_font_info, "", ""),
#endif
);
