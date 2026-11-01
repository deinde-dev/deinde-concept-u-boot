// SPDX-License-Identifier: GPL-2.0+
/*
 * Simplefb device tree support
 *
 * (C) Copyright 2015
 * Stephen Warren <swarren@wwwdotorg.org>
 */

#define LOG_CATEGORY	LOGC_BOOT

#include <dm.h>
#include <fdt_support.h>
#include <linux/libfdt.h>
#include <video.h>
#include <spl.h>
#include <bloblist.h>

/**
 * simplefb_format_name() - Get the simple-framebuffer name for a pixel format
 *
 * @bpix: log2 of bits per pixel
 * @format: Pixel format, or VIDEO_UNKNOWN to fall back to the depth alone
 * Return: Format name as used in the 'format' property, or NULL if unsupported
 */
static const char *simplefb_format_name(int bpix, enum video_format format)
{
	switch (format) {
	case VIDEO_X8R8G8B8:
		return "x8r8g8b8";
	case VIDEO_X8B8G8R8:
		return "x8b8g8r8";
	case VIDEO_X2R10G10B10:
		return "x2r10g10b10";
	case VIDEO_RGBA8888:
		return "a8b8g8r8";
	default:
		break;
	}

	switch (bpix) {
	case VIDEO_BPP16:
		return "r5g6b5";
	case VIDEO_BPP32:
		return "a8r8g8b8";
	default:
		return NULL;
	}
}

static int fdt_simplefb_configure_node(void *blob, int off)
{
	int xsize, ysize;
	int bpix; /* log2 of bits per pixel */
	enum video_format format;
	const char *name;
	ulong fb_base;
	struct video_uc_plat *plat;
	struct video_priv *uc_priv;
	struct udevice *dev;
	int ret;

	if (IS_ENABLED(CONFIG_SPL_VIDEO_HANDOFF) && not_xpl()) {
		struct video_handoff *ho;

		ho = bloblist_find(BLOBLISTT_U_BOOT_VIDEO, sizeof(*ho));
		if (!ho)
			return log_msg_ret("Missing video bloblist", -ENOENT);

		xsize = ho->xsize;
		ysize = ho->ysize;
		bpix = ho->bpix;
		fb_base = ho->fb;
		format = ho->format;
	} else {
		ret = uclass_first_device_err(UCLASS_VIDEO, &dev);
		if (ret)
			return ret;
		uc_priv = dev_get_uclass_priv(dev);
		plat = dev_get_uclass_plat(dev);
		xsize = uc_priv->xsize;
		ysize = uc_priv->ysize;
		bpix = uc_priv->bpix;
		fb_base = plat->base;
		format = uc_priv->format;
		log_debug("simplefb: fb %lx x %d y %d bpix %x\n", fb_base,
			  xsize, ysize, bpix);
	}

	name = simplefb_format_name(bpix, format);
	if (!name)
		return -EINVAL;

	return fdt_setup_simplefb_node(blob, off, fb_base, xsize, ysize,
				       xsize * (1 << bpix) / 8, name);
}

int fdt_simplefb_add_node(void *blob)
{
	static const char compat[] = "simple-framebuffer";
	static const char disabled[] = "disabled";
	int off, ret;

	off = fdt_add_subnode(blob, 0, "framebuffer");
	if (off < 0)
		return -1;

	ret = fdt_setprop(blob, off, "status", disabled, sizeof(disabled));
	if (ret < 0)
		return -1;

	ret = fdt_setprop(blob, off, "compatible", compat, sizeof(compat));
	if (ret < 0)
		return -1;

	return fdt_simplefb_configure_node(blob, off);
}

/**
 * fdt_simplefb_enable_existing_node() - enable simple-framebuffer DT node
 *
 * @blob:	device-tree
 * Return:	0 on success, non-zero otherwise
 */
static int fdt_simplefb_enable_existing_node(void *blob)
{
	int off;

	off = fdt_node_offset_by_compatible(blob, -1, "simple-framebuffer");
	if (off < 0)
		return -1;

	return fdt_simplefb_configure_node(blob, off);
}

int fdt_simplefb_enable_and_mem_rsv(void *blob)
{
	int ret;

	/* nothing to do when video is not active */
	if (!video_is_active())
		return 0;

	ret = fdt_simplefb_enable_existing_node(blob);
	if (ret)
		return ret;

	return fdt_add_fb_mem_rsv(blob);
}

/**
 * fdt_simplefb_add_chosen_node() - Add a simple-framebuffer node under /chosen
 *
 * The binding requires the node to be under /chosen, with its 'reg' in the
 * root's address space, so /chosen is given the root's cell sizes and an
 * empty 'ranges' property.
 *
 * @blob: Devicetree to update
 * Return: Offset of the new node, or -ve libfdt error
 */
static int fdt_simplefb_add_chosen_node(void *blob)
{
	int chosen, off, addrc, sizec, ret;

	chosen = fdt_find_or_add_subnode(blob, 0, "chosen");
	if (chosen < 0)
		return chosen;

	fdt_support_default_count_cells(blob, 0, &addrc, &sizec);
	ret = fdt_setprop_u32(blob, chosen, "#address-cells", addrc);
	if (!ret)
		ret = fdt_setprop_u32(blob, chosen, "#size-cells", sizec);
	if (!ret)
		ret = fdt_setprop_empty(blob, chosen, "ranges");
	if (ret)
		return ret;

	off = fdt_add_subnode(blob, chosen, "framebuffer");
	if (off < 0)
		return off;
	ret = fdt_setprop_string(blob, off, "compatible", "simple-framebuffer");
	if (ret)
		return ret;

	return off;
}

int fdt_simplefb_handoff(void *blob)
{
	int off, ret;

	/* nothing to do when video is not active */
	if (!video_is_active())
		return 0;

	off = fdt_node_offset_by_compatible(blob, -1, "simple-framebuffer");
	if (off < 0) {
		off = fdt_simplefb_add_chosen_node(blob);
		if (off < 0)
			return off;
	}
	ret = fdt_simplefb_configure_node(blob, off);
	if (ret)
		return ret;

	return fdt_add_fb_mem_rsv(blob);
}
