// SPDX-License-Identifier: GPL-2.0+
/*
 * Verified Boot for Embedded (VBE) 'abrec' method (for firmware)
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_BOOT

#include <blk.h>
#include <bloblist.h>
#include <bootmeth.h>
#include <dm.h>
#include <malloc.h>
#include <memalign.h>
#include <mmc.h>
#include <spl.h>
#include <linux/libfdt.h>
#include <dm/ofnode.h>
#include "vbe_abrec.h"

static const char *const result_names[] = {"unknown", "trying", "ok", "bad"};
static const char *const pick_names[VBEP_COUNT] = {"A", "B", "Recovery"};

int abrec_read_priv(ofnode node, struct abrec_priv *priv)
{
	memset(priv, '\0', sizeof(*priv));
	if (ofnode_read_u32(node, "area-start", &priv->area_start) ||
	    ofnode_read_u32(node, "area-size", &priv->area_size) ||
	    ofnode_read_u32(node, "state-offset", &priv->state_offset) ||
	    ofnode_read_u32(node, "state-size", &priv->state_size))
		return log_msg_ret("read", -EINVAL);
	ofnode_read_u32(node, "skip-offset", &priv->skip_offset);

	/* the version comes from the slot info if there is any, else from here */
	ofnode_read_u32(node, "version-offset", &priv->version_offset);
	ofnode_read_u32(node, "version-size", &priv->version_size);

	/* only U-Boot proper reads the slot info; VPL finds slots with binman */
	if (xpl_phase() >= PHASE_BOARD_F) {
		ofnode_read_u32(node, "slot-a-offset", &priv->slot_a);
		ofnode_read_u32(node, "slot-b-offset", &priv->slot_b);
		ofnode_read_u32(node, "slot-recovery-offset",
				&priv->slot_recovery);
		ofnode_read_u32(node, "slot-info-offset",
				&priv->slot_info_offset);
		ofnode_read_u32(node, "slot-info-size", &priv->slot_info_size);
	}
	priv->storage = strdup(ofnode_read_string(node, "storage"));
	if (!priv->storage)
		return log_msg_ret("str", -EINVAL);

	return 0;
}

int abrec_read_nvdata(struct abrec_priv *priv, struct udevice *blk,
		      struct abrec_state *state)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, buf, MMC_MAX_BLOCK_LEN);
	const struct vbe_nvdata *nvd = (struct vbe_nvdata *)buf;
	uint flags;
	int ret;

	ret = vbe_read_nvdata(blk, priv->area_start + priv->state_offset,
			      priv->state_size, buf);
	if (ret == -EPERM) {
		memset(buf, '\0', MMC_MAX_BLOCK_LEN);
		log_warning("Starting with empty state\n");
	} else if (ret) {
		return log_msg_ret("nv", ret);
	}

	state->fw_vernum = nvd->fw_vernum;
	flags = nvd->flags;
	state->try_count = flags & VBEF_TRY_COUNT_MASK;
	state->try_b = flags & VBEF_TRY_B;
	state->recovery = flags & VBEF_RECOVERY;
	state->try_result = (flags & VBEF_RESULT_MASK) >> VBEF_RESULT_SHIFT;
	state->pick = (flags & VBEF_PICK_MASK) >> VBEF_PICK_SHIFT;

	return 0;
}

int abrec_write_nvdata(struct abrec_priv *priv, struct udevice *blk,
		       const struct abrec_state *state)
{
	struct vbe_nvdata nvd;
	int ret;

	memset(&nvd, '\0', sizeof(nvd));
	nvd.fw_vernum = state->fw_vernum;
	nvd.flags = (state->try_count & VBEF_TRY_COUNT_MASK) |
		(state->try_b ? VBEF_TRY_B : 0) |
		(state->recovery ? VBEF_RECOVERY : 0) |
		(state->try_result << VBEF_RESULT_SHIFT & VBEF_RESULT_MASK) |
		(state->pick << VBEF_PICK_SHIFT & VBEF_PICK_MASK);

	ret = vbe_write_nvdata(blk, priv->area_start + priv->state_offset,
			       priv->state_size, &nvd);
	if (ret)
		return log_msg_ret("wr", ret);

	return 0;
}

enum vbe_pick_t abrec_choose(struct abrec_state *state, bool *changedp)
{
	struct abrec_state old = *state;
	enum vbe_pick_t pick;

	if (state->recovery) {
		pick = VBEP_RECOVERY;
	} else if (state->try_b && state->try_count) {
		/* use up one of the tries; the OS marks B good if it boots */
		pick = VBEP_B;
		state->try_count--;
		state->try_result = VBETR_TRYING;
	} else if (state->try_b) {
		/* B never got as far as the OS, so give up on it */
		pick = VBEP_A;
		state->try_b = false;
		state->try_result = VBETR_BAD;
	} else {
		pick = VBEP_A;
	}
	state->pick = pick;

	*changedp = old.try_count != state->try_count ||
		old.try_b != state->try_b ||
		old.try_result != state->try_result ||
		old.pick != state->pick;

	return pick;
}

enum vbe_pick_t abrec_fallback(struct abrec_state *state,
			       enum vbe_pick_t failed, bool *changedp)
{
	enum vbe_pick_t pick;

	switch (failed) {
	case VBEP_B:
		state->try_b = false;
		state->try_count = 0;
		state->try_result = VBETR_BAD;
		pick = VBEP_A;
		break;
	case VBEP_A:
		pick = VBEP_RECOVERY;
		break;
	default:
		return VBEP_COUNT;
	}
	state->pick = pick;
	*changedp = true;

	return pick;
}

enum vbe_pick_t abrec_get_pick(void)
{
	struct vbe_handoff *handoff;

	handoff = bloblist_find(BLOBLISTT_VBE, sizeof(*handoff));
	if (!handoff || handoff->pick >= VBEP_COUNT)
		return VBEP_COUNT;

	return handoff->pick;
}

/**
 * abrec_read_slot_version() - Read the version from a slot's slot info
 *
 * The slot info is a small devicetree after the firmware in each slot, which
 * the updater writes along with the firmware. Its root node has the compatible
 * string "fwupd,vbe-slot-info" and a 'version' property, among others. A slot
 * with no valid slot info gives an empty version
 *
 * @priv: abrec information
 * @blk: Block device holding the firmware
 * @slot: Offset of the slot in the device, in bytes
 * @version: Returns the version, or an empty string if not known
 * @maxlen: Size of @version
 * Return: 0 if OK, -ve on error
 */
static int abrec_read_slot_version(struct abrec_priv *priv,
				   struct udevice *blk, ulong slot,
				   char *version, int maxlen)
{
	struct blk_desc *desc = dev_get_uclass_plat(blk);
	ulong start = slot + priv->slot_info_offset;
	const char *prop;
	ulong blks;
	void *buf;
	int len;

	*version = '\0';
	if (start % desc->blksz || priv->slot_info_size % desc->blksz)
		return log_msg_ret("ali", -EINVAL);
	buf = memalign(ARCH_DMA_MINALIGN, priv->slot_info_size);
	if (!buf)
		return log_msg_ret("buf", -ENOMEM);
	blks = priv->slot_info_size / desc->blksz;
	if (blk_read(blk, start / desc->blksz, blks, buf) != blks) {
		free(buf);
		return log_msg_ret("rd", -EIO);
	}
	if (!fdt_check_header(buf) &&
	    fdt_totalsize(buf) <= priv->slot_info_size &&
	    !fdt_node_check_compatible(buf, 0, "fwupd,vbe-slot-info")) {
		prop = fdt_getprop(buf, 0, "version", &len);
		if (prop && len > 0)
			strlcpy(version, prop, min(len, maxlen));
	}
	free(buf);

	return 0;
}

int abrec_read_state(struct udevice *dev, struct abrec_state *state)
{
	struct abrec_priv *priv = dev_get_priv(dev);
	struct udevice *blk;
	ulong slot;
	int ret;

	ret = vbe_get_blk(priv->storage, &blk);
	if (ret)
		return log_msg_ret("blk", ret);

	/* report the version of the firmware which booted, if it says */
	if (priv->slot_info_size) {
		switch (abrec_get_pick()) {
		case VBEP_B:
			slot = priv->slot_b;
			break;
		case VBEP_RECOVERY:
			slot = priv->slot_recovery;
			break;
		default:
			slot = priv->slot_a;
			break;
		}
		ret = abrec_read_slot_version(priv, blk, slot,
					      state->fw_version,
					      MAX_VERSION_LEN);
	} else if (priv->version_size) {
		ret = vbe_read_version(blk,
				       priv->area_start + priv->version_offset,
				       state->fw_version, MAX_VERSION_LEN);
	} else {
		*state->fw_version = '\0';
		ret = 0;
	}
	if (ret)
		return log_msg_ret("ver", ret);
	log_debug("version=%s\n", state->fw_version);

	ret = abrec_read_nvdata(priv, blk, state);
	if (ret)
		return log_msg_ret("nvd", ret);

	return 0;
}

int vbe_abrec_get_recovery(struct udevice *dev, bool *recoveryp)
{
	struct abrec_state state;
	int ret;

	if (dev->driver != DM_DRIVER_GET(vbe_abrec))
		return -ENOSYS;
	ret = abrec_read_state(dev, &state);
	if (ret)
		return log_msg_ret("vgr", ret);
	*recoveryp = state.recovery;

	return 0;
}

int vbe_abrec_set_recovery(struct udevice *dev, bool recovery)
{
	struct abrec_priv *priv = dev_get_priv(dev);
	struct abrec_state state;
	struct udevice *blk;
	int ret;

	if (dev->driver != DM_DRIVER_GET(vbe_abrec))
		return -ENOSYS;
	ret = vbe_get_blk(priv->storage, &blk);
	if (ret)
		return log_msg_ret("vsb", ret);
	ret = abrec_read_nvdata(priv, blk, &state);
	if (ret)
		return log_msg_ret("vsr", ret);
	state.recovery = recovery;
	ret = abrec_write_nvdata(priv, blk, &state);
	if (ret)
		return log_msg_ret("vsw", ret);

	return 0;
}

static int vbe_abrec_get_state_desc(struct udevice *dev, char *buf,
				    int maxsize)
{
	struct abrec_state state;
	int ret;

	/* nothing shows this before U-Boot proper, so save the space */
	if (IS_ENABLED(CONFIG_XPL_BUILD))
		return -ENOSYS;

	ret = abrec_read_state(dev, &state);
	if (ret)
		return log_msg_ret("read", ret);

	if (maxsize < 30)
		return -ENOSPC;
	snprintf(buf, maxsize,
		 "Version: %s\nVernum: %x/%x\nTry B: %s, %d tries left\n"
		 "Last result: %s\nRecovery: %s\nPicked: %s", state.fw_version,
		 state.fw_vernum >> FWVER_KEY_SHIFT,
		 state.fw_vernum & FWVER_FW_MASK, state.try_b ? "yes" : "no",
		 state.try_count, result_names[state.try_result],
		 state.recovery ? "on" : "off",
		 state.pick < VBEP_COUNT ? pick_names[state.pick] : "?");

	return 0;
}

static int vbe_abrec_read_bootflow(struct udevice *dev, struct bootflow *bflow)
{
	int ret;

	if (IS_ENABLED(CONFIG_BOOTMETH_VBE_ABREC_FW)) {
		if (vbe_phase() == VBE_PHASE_FIRMWARE) {
			ret = abrec_read_bootflow_fw(dev, bflow);
			if (ret)
				return log_msg_ret("fw", ret);
			return 0;
		}
	}

	return -EINVAL;
}

static int vbe_abrec_read_file(struct udevice *dev, struct bootflow *bflow,
			       const char *file_path, ulong *addrp, ulong align,
			       enum bootflow_img_t type, ulong *sizep)
{
	int ret;

	if (vbe_phase() == VBE_PHASE_OS) {
		ret = bootmeth_common_read_file(dev, bflow, file_path, addrp,
						align, type, sizep);
		if (ret)
			return log_msg_ret("os", ret);
	}

	/* To be implemented */
	return -EINVAL;
}

static struct bootmeth_ops bootmeth_vbe_abrec_ops = {
	.get_state_desc	= vbe_abrec_get_state_desc,
	.read_bootflow	= vbe_abrec_read_bootflow,
	.read_file	= vbe_abrec_read_file,
};

static int bootmeth_vbe_abrec_probe(struct udevice *dev)
{
	struct abrec_priv *priv = dev_get_priv(dev);
	int ret;

	ret = abrec_read_priv(dev_ofnode(dev), priv);
	if (ret)
		return log_msg_ret("abp", ret);

	return 0;
}

static int bootmeth_vbe_abrec_bind(struct udevice *dev)
{
	struct bootmeth_uc_plat *plat = dev_get_uclass_plat(dev);

	plat->desc = "VBE A/B/recovery";
	plat->flags = BOOTMETHF_GLOBAL;

	return 0;
}

#if IS_ENABLED(CONFIG_OF_REAL)
static const struct udevice_id vbe_abrec_ids[] = {
	{ .compatible = "fwupd,vbe-abrec" },
	{ }
};
#endif

U_BOOT_DRIVER(vbe_abrec) = {
	.name	= "vbe_abrec",
	.id	= UCLASS_BOOTMETH,
	.of_match = of_match_ptr(vbe_abrec_ids),
	.ops	= &bootmeth_vbe_abrec_ops,
	.bind	= bootmeth_vbe_abrec_bind,
	.probe	= bootmeth_vbe_abrec_probe,
	.flags	= DM_FLAG_PRE_RELOC,
	.priv_auto	= sizeof(struct abrec_priv),
};
