// SPDX-License-Identifier: GPL-2.0+
/*
 * Verified Boot for Embedded (VBE) 'abrec' method (for firmware)
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_BOOT

#include <bootmeth.h>
#include <dm.h>
#include <memalign.h>
#include <mmc.h>
#include <dm/ofnode.h>
#include "vbe_abrec.h"

int abrec_read_priv(ofnode node, struct abrec_priv *priv)
{
	memset(priv, '\0', sizeof(*priv));
	if (ofnode_read_u32(node, "area-start", &priv->area_start) ||
	    ofnode_read_u32(node, "area-size", &priv->area_size) ||
	    ofnode_read_u32(node, "version-offset", &priv->version_offset) ||
	    ofnode_read_u32(node, "version-size", &priv->version_size) ||
	    ofnode_read_u32(node, "state-offset", &priv->state_offset) ||
	    ofnode_read_u32(node, "state-size", &priv->state_size))
		return log_msg_ret("read", -EINVAL);
	ofnode_read_u32(node, "skip-offset", &priv->skip_offset);
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

int abrec_read_state(struct udevice *dev, struct abrec_state *state)
{
	struct abrec_priv *priv = dev_get_priv(dev);
	struct udevice *blk;
	int ret;

	ret = vbe_get_blk(priv->storage, &blk);
	if (ret)
		return log_msg_ret("blk", ret);

	ret = vbe_read_version(blk, priv->area_start + priv->version_offset,
			       state->fw_version, MAX_VERSION_LEN);
	if (ret)
		return log_msg_ret("ver", ret);
	log_debug("version=%s\n", state->fw_version);

	ret = abrec_read_nvdata(priv, blk, state);
	if (ret)
		return log_msg_ret("nvd", ret);

	return 0;
}

static int vbe_abrec_get_state_desc(struct udevice *dev, char *buf,
				    int maxsize)
{
	struct abrec_state state;
	int ret;

	ret = abrec_read_state(dev, &state);
	if (ret)
		return log_msg_ret("read", ret);

	if (maxsize < 30)
		return -ENOSPC;
	snprintf(buf, maxsize, "Version: %s\nVernum: %x/%x", state.fw_version,
		 state.fw_vernum >> FWVER_KEY_SHIFT,
		 state.fw_vernum & FWVER_FW_MASK);

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
