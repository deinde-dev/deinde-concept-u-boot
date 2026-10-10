// SPDX-License-Identifier: GPL-2.0+
/*
 * Verified Boot for Embedded (VBE) 'abrec' method: information for the OS
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_BOOT

#include <bloblist.h>
#include <dm.h>
#include <event.h>
#include <vbe.h>
#include <version_string.h>
#include <dm/device-internal.h>
#include <dm/ofnode.h>
#include "vbe_abrec.h"

static const char *const slot_names[] = {"a", "b", "recovery"};

int abrec_fixup_node(ofnode node, const struct abrec_state *state,
		     enum vbe_pick_t pick)
{
	const char *version, *str;
	int ret;

	/*
	 * An erased or never-written version block holds no string, so leave
	 * the version out and let the updater treat the firmware as unknown
	 */
	for (str = state->fw_version; *str; str++) {
		uchar ch = *str;

		/* isprint() accepts Latin-1, which an erased 0xff would pass */
		if (ch < ' ' || ch > '~')
			break;
	}
	if (*state->fw_version && !*str) {
		/* the live tree keeps a pointer to the string */
		version = strdup(state->fw_version);
		if (!version)
			return log_msg_ret("dup", -ENOMEM);
		ret = ofnode_write_string(node, "cur-version", version);
		if (ret)
			return log_msg_ret("ver", ret);
	}
	ret = ofnode_write_u32(node, "cur-vernum", state->fw_vernum);
	if (ret)
		return log_msg_ret("num", ret);

	/* Drop the 'U-Boot ' at the start */
	str = version_string;
	if (!strncmp("U-Boot ", str, 7))
		str += 7;
	ret = ofnode_write_string(node, "bootloader-version", str);
	if (ret)
		return log_msg_ret("bl", ret);

	if (pick < VBEP_COUNT) {
		ret = ofnode_write_string(node, "booted-slot",
					  slot_names[pick]);
		if (ret)
			return log_msg_ret("slt", ret);
	}

	return 0;
}

int abrec_add_compatible(oftree tree, ofnode node)
{
	const void *compat;
	int len;

	if (ofnode_has_property(node, "compatible"))
		return 0;
	compat = ofnode_read_prop(oftree_root(tree), "compatible", &len);
	if (!compat)
		return 0;

	return ofnode_write_prop(node, "compatible", compat, len, true);
}

/**
 * abrec_get_pick() - Find out which firmware VPL chose for this boot
 *
 * Return: Firmware picked, or VBEP_COUNT if not known
 */
static enum vbe_pick_t abrec_get_pick(void)
{
	struct vbe_handoff *handoff;

	handoff = bloblist_find(BLOBLISTT_VBE, sizeof(*handoff));
	if (!handoff || handoff->pick >= VBEP_COUNT)
		return VBEP_COUNT;

	return handoff->pick;
}

/**
 * abrec_ft_fixup() - Write out the VBE abrec information for fwupd
 *
 * This adds a node under /chosen/fwupd for each abrec bootmeth, containing
 * its devicetree properties along with the current version and the firmware
 * slot which was booted
 *
 * @ctx: Context for event
 * @event: Event to process
 * Return: 0 if OK, -ve on error
 */
static int abrec_ft_fixup(void *ctx, struct event *event)
{
	oftree tree = event->data.ft_fixup.tree;
	struct udevice *dev;

	for (vbe_find_first_device(&dev); dev; vbe_find_next_device(&dev)) {
		struct abrec_state state;
		ofnode node, subnode, chosen;
		int ret;

		if (strcmp("vbe_abrec", dev->driver->name))
			continue;

		chosen = oftree_path(tree, "/chosen");
		if (!ofnode_valid(chosen))
			continue;

		ret = device_probe(dev);
		if (ret) {
			log_debug("VBE device '%s' failed to probe (err=%d)",
				  dev->name, ret);
			return 0;
		}

		ret = ofnode_add_subnode(chosen, "fwupd", &node);
		if (ret && ret != -EEXIST)
			return log_msg_ret("fwu", ret);

		ret = abrec_add_compatible(tree, node);
		if (ret)
			return log_msg_ret("cmp", ret);

		ret = ofnode_add_subnode(node, dev->name, &subnode);
		if (ret && ret != -EEXIST)
			return log_msg_ret("dev", ret);

		/* Copy over the vbe properties for fwupd */
		log_debug("Fixing up: %s\n", dev->name);
		ret = ofnode_copy_props(subnode, dev_ofnode(dev));
		if (ret)
			return log_msg_ret("cp", ret);

		ret = abrec_read_state(dev, &state);
		if (ret)
			return log_msg_ret("rd", ret);

		ret = abrec_fixup_node(subnode, &state, abrec_get_pick());
		if (ret)
			return log_msg_ret("fix", ret);
	}

	return 0;
}
EVENT_SPY_FULL(EVT_FT_FIXUP, abrec_ft_fixup);
