// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the cls command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <dm.h>
#include <pager.h>
#include <video.h>
#include <asm/global_data.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Sequence the command sends to the terminal: clear the screen, then home */
#define CLS_ANSI	"\x1b[2J\x1b[1;1H"

/* Line count the pager test starts from */
#define CLS_LINES	7

/**
 * make_pixel() - Lay out a colour as the bytes of one pixel
 *
 * The value is written through a pointer of the size the display uses, so the
 * bytes come out in the order the frame buffer holds them whatever the byte
 * order of the machine
 *
 * @priv: Video device's uclass private data
 * @colour: Pixel value to lay out
 * @pix: Buffer to hold the bytes, which must be at least 4 bytes long
 * Return: 0 if OK, -EAGAIN if the depth is not one the test knows
 */
static int make_pixel(struct video_priv *priv, u32 colour, void *pix)
{
	switch (priv->bpix) {
	case VIDEO_BPP8:
		*(u8 *)pix = colour;
		break;
	case VIDEO_BPP16:
		*(u16 *)pix = colour;
		break;
	case VIDEO_BPP32:
		*(u32 *)pix = colour;
		break;
	default:
		return -EAGAIN;
	}

	return 0;
}

/**
 * fill_display() - Write a pattern over the whole frame buffer
 *
 * Every pixel becomes the inverse of the background colour, so that a test can
 * tell that the command has cleared the display rather than left it alone. The
 * background is white on a U-Boot display, so filling with 0xff bytes would
 * prove nothing
 *
 * @uts: Test state
 * @devp: Returns the video device, if there is one
 * Return: 0 if OK, -EAGAIN if there is no display or its depth is not one the
 * test knows, other -ve on error
 */
static int fill_display(struct unit_test_state *uts, struct udevice **devp)
{
	struct video_priv *priv;
	struct udevice *dev;
	uint bytes, offset;
	u32 pix;
	int ret;

	if (uclass_first_device_err(UCLASS_VIDEO, &dev))
		return -EAGAIN;
	priv = dev_get_uclass_priv(dev);
	ret = make_pixel(priv, ~priv->colour_bg, &pix);
	if (ret)
		return ret;

	bytes = VNBYTES(priv->bpix);
	for (offset = 0; offset + bytes <= priv->fb_size; offset += bytes)
		memcpy(priv->fb + offset, &pix, bytes);
	*devp = dev;

	return 0;
}

/**
 * check_display() - Check that every pixel holds the background colour
 *
 * @uts: Test state
 * @dev: Video device
 * Return: 0 if OK, -ve on error
 */
static int check_display(struct unit_test_state *uts, struct udevice *dev)
{
	struct video_priv *priv = dev_get_uclass_priv(dev);
	uint bytes, offset;
	u8 *fb = priv->fb;
	u32 bg;

	ut_assertok(make_pixel(priv, priv->colour_bg, &bg));

	bytes = VNBYTES(priv->bpix);
	for (offset = 0; offset + bytes <= priv->fb_size; offset += bytes) {
		ut_assertf(!memcmp(fb + offset, &bg, bytes),
			   "pixel at offset %x is not the background colour\n",
			   offset);
	}

	return 0;
}

/*
 * Test 'cls'
 *
 * The command clears a video display by sending the escape sequence to the
 * console, which the video console decodes, so the test needs UTF_NO_SILENT:
 * a silenced console records the sequence but sends it nowhere, leaving the
 * display untouched
 */
static int cmd_test_cls_base(struct unit_test_state *uts)
{
	struct udevice *dev;
	int ret;

	ret = fill_display(uts, &dev);
	if (ret == -EAGAIN)
		return ret;
	ut_assertok(ret);

	/* the command sends the sequence without a newline after it */
	ut_assertok(run_command("cls", 0));
	ut_assert_nextline(CLS_ANSI);
	ut_assert_console_end();

	ut_assertok(check_display(uts, dev));

	return 0;
}
CMD_TEST(cmd_test_cls_base, UTF_CONSOLE | UTF_NO_SILENT);

/* Test that the command starts a new page */
static int cmd_test_cls_pager(struct unit_test_state *uts)
{
	struct pager *pag = gd_pager();

	if (!pag)
		return -EAGAIN;

	/*
	 * the pager is bypassed while tests run, so no amount of output moves
	 * the line count; set it by hand instead
	 */
	pag->line_count = CLS_LINES;

	ut_assertok(run_command("cls", 0));
	ut_assert_nextline(CLS_ANSI);
	ut_assert_console_end();

	ut_asserteq(0, pag->line_count);

	return 0;
}
CMD_TEST(cmd_test_cls_pager, UTF_CONSOLE);

/* Test 'cls' with an argument, which it does not take */
static int cmd_test_cls_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("cls extra", 0));
	ut_assert_nextline("cls - clear screen");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextlinen("cls");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_cls_usage, UTF_CONSOLE);
