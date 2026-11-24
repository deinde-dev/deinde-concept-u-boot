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
#include <linker_lists.h>
#include <video.h>
#include <video_console.h>

static int do_video_setcursor(struct getopt_state *gs)
{
	char *const *argv = gs->argv;
	unsigned int col, row;
	struct udevice *dev;

	if (gs->argc != 3)
		return CMD_RET_USAGE;

	if (uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &dev))
		return CMD_RET_FAILURE;
	col = hextoul(argv[1], NULL);
	row = hextoul(argv[2], NULL);
	vidconsole_position_cursor(dev, col, row);

	return 0;
}

static int do_video_puts(struct getopt_state *gs)
{
	char *const *argv = gs->argv;
	struct udevice *dev;
	int ret;

	if (gs->argc != 2)
		return CMD_RET_USAGE;

	if (uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &dev))
		return CMD_RET_FAILURE;
	ret = vidconsole_put_string(dev, NULL, argv[1]);
	if (!ret)
		ret = video_sync(dev->parent, false);

	return ret ? CMD_RET_FAILURE : 0;
}

static int do_video_write(struct getopt_state *gs)
{
	struct vidconsole_priv *priv;
	char *const *argv = gs->argv;
	bool use_pixels = false;
	int argc = gs->argc;
	struct udevice *dev;
	int opt, ret, i;

	while ((opt = getopt(gs, "+p")) > 0) {
		switch (opt) {
		case 'p':
			use_pixels = true;
			break;
		default:
			return CMD_RET_USAGE;
		}
	}

	/* each position needs a string to go with it */
	if (argc - gs->index < 2)
		return CMD_RET_USAGE;

	if (uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &dev))
		return CMD_RET_FAILURE;
	priv = dev_get_uclass_priv(dev);

	if ((argc - gs->index) % 2)
		return CMD_RET_USAGE;

	for (i = gs->index; i < argc; i += 2) {
		uint col, row;
		char *pos = argv[i];
		char *colon = strchr(pos, ':');

		if (!colon)
			return CMD_RET_USAGE;

		col = hextoul(pos, NULL);
		row = hextoul(colon + 1, NULL);

		if (use_pixels)
			vidconsole_set_cursor_pos(dev, NULL, col, row);
		else
			vidconsole_position_cursor(dev, col, row);

		ret = vidconsole_put_string(dev, NULL, argv[i + 1]);
		if (ret)
			return CMD_RET_FAILURE;
	}

	ret = video_sync(dev->parent, false);
	if (ret)
		return CMD_RET_FAILURE;

	return 0;
}

static int do_video_images(struct getopt_state *gs)
{
	struct video_image *image;
	int count, i;

	image = ll_entry_start(struct video_image, video_image);
	count = ll_entry_count(struct video_image, video_image);

	printf("%-20s %10s\n", "Name", "Size");
	printf("%-20s %10s\n", "--------------------", "----------");

	for (i = 0; i < count; i++, image++) {
		ulong size = (ulong)image->end - (ulong)image->begin;

		printf("%-20s %10lu\n", image->name, size);
	}

	printf("\nTotal images: %d\n", count);

	return 0;
}

U_BOOT_CMD_NOOPTS(
	setcurs, 3,	1,	do_video_setcursor,
	"set cursor position within screen",
	"    <col> <row> in hex characters"
);

U_BOOT_CMD_NOOPTS(
	lcdputs, 2,	1,	do_video_puts,
	"print string on video framebuffer",
	"    <string>"
);

U_BOOT_LONGHELP(video,
	"setcursor <col> <row>                - Set cursor position\n"
	"video puts <string>                        - Write string at current position\n"
	"video write [-p] [<col>:<row> <string>]... - Write strings at specified positions\n"
	"         -p: Use pixel coordinates instead of character positions\n"
	"video images                               - List images compiled into U-Boot");

U_BOOT_CMD_WITH_SUBCMDS(video, "Video commands", video_help_text,
	U_BOOT_CMD_MKENT_NOOPTS(setcursor, 3, 1, do_video_setcursor, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(puts, 2, 1, do_video_puts, "", ""),
	U_BOOT_CMD_MKENT_GETOPT(write, CONFIG_SYS_MAXARGS, 1, do_video_write,
				"", ""),
	U_BOOT_CMD_MKENT_NOOPTS(images, 1, 1, do_video_images, "", ""));
