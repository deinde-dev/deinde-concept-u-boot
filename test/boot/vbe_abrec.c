// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for the vbe-abrec firmware state. All start with 'vbe_abrec'
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <blk.h>
#include <dm.h>
#include <memalign.h>
#include <mmc.h>
#include <vbe.h>
#include <version_string.h>
#include <dm/device-internal.h>
#include <linux/libfdt.h>
#include <dm/ofnode.h>
#include <test/ut.h>
#include "bootstd_common.h"
#include "../../boot/vbe_abrec.h"

/* Check the choice of firmware and the state which is written back */
static int vbe_abrec_test_choose(struct unit_test_state *uts)
{
	struct abrec_state state;
	bool changed;

	/* nothing requested, so boot A and leave the state alone */
	memset(&state, '\0', sizeof(state));
	ut_asserteq(VBEP_A, abrec_choose(&state, true, &changed));
	ut_assert(!changed);

	/* an update asks to try B three times */
	state.try_b = true;
	state.try_count = 3;
	ut_asserteq(VBEP_B, abrec_choose(&state, true, &changed));
	ut_assert(changed);
	ut_asserteq(2, state.try_count);
	ut_asserteq(VBETR_TRYING, state.try_result);
	ut_asserteq(VBEP_B, state.pick);

	/* B fails to reach the OS twice more, using up the tries */
	ut_asserteq(VBEP_B, abrec_choose(&state, true, &changed));
	ut_asserteq(VBEP_B, abrec_choose(&state, true, &changed));
	ut_asserteq(0, state.try_count);

	/* so it is marked bad and A is used */
	ut_asserteq(VBEP_A, abrec_choose(&state, true, &changed));
	ut_assert(changed);
	ut_assert(!state.try_b);
	ut_asserteq(VBETR_BAD, state.try_result);
	ut_asserteq(VBEP_A, state.pick);

	/* after that, A is used without any change */
	ut_asserteq(VBEP_A, abrec_choose(&state, true, &changed));
	ut_assert(!changed);

	/* recovery overrides everything */
	state.recovery = true;
	state.try_b = true;
	state.try_count = 1;
	ut_asserteq(VBEP_RECOVERY, abrec_choose(&state, true, &changed));
	ut_asserteq(1, state.try_count);

	/* a board with only A and B carries on as if recovery were not asked */
	ut_asserteq(VBEP_B, abrec_choose(&state, false, &changed));
	ut_asserteq(0, state.try_count);
	ut_asserteq(VBEP_B, state.pick);

	return 0;
}
BOOTSTD_TEST(vbe_abrec_test_choose, 0);

/* Check what is used when the chosen firmware cannot be loaded */
static int vbe_abrec_test_fallback(struct unit_test_state *uts)
{
	struct abrec_state state;
	bool changed;

	/* a B which cannot be loaded is marked bad at once, without its tries */
	memset(&state, '\0', sizeof(state));
	state.try_b = true;
	state.try_count = 3;
	ut_asserteq(VBEP_B, abrec_choose(&state, true, &changed));
	changed = false;
	ut_asserteq(VBEP_A, abrec_fallback(&state, VBEP_B, true, &changed));
	ut_assert(changed);
	ut_assert(!state.try_b);
	ut_asserteq(0, state.try_count);
	ut_asserteq(VBETR_BAD, state.try_result);
	ut_asserteq(VBEP_A, state.pick);

	/* if A cannot be loaded either, recovery is used */
	changed = false;
	ut_asserteq(VBEP_RECOVERY, abrec_fallback(&state, VBEP_A, true, &changed));
	ut_assert(changed);
	ut_asserteq(VBEP_RECOVERY, state.pick);
	ut_asserteq(VBETR_BAD, state.try_result);

	/* there is nothing after recovery */
	changed = false;
	ut_asserteq(VBEP_COUNT, abrec_fallback(&state, VBEP_RECOVERY, true,
					       &changed));
	ut_assert(!changed);
	ut_asserteq(VBEP_RECOVERY, state.pick);

	/* with only A and B, there is nothing to try once A cannot be loaded */
	changed = false;
	ut_asserteq(VBEP_COUNT, abrec_fallback(&state, VBEP_A, false, &changed));
	ut_assert(!changed);

	return 0;
}
BOOTSTD_TEST(vbe_abrec_test_fallback, 0);

/* Check writing the state and reading it back */
static int vbe_abrec_test_nvdata(struct unit_test_state *uts)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, orig, MMC_MAX_BLOCK_LEN);
	struct abrec_state state, check;
	struct abrec_priv priv;
	struct udevice *blk;
	ofnode node;

	/* firmware0 in the sandbox devicetree has the same properties */
	node = ofnode_path("/bootstd/firmware0");
	ut_assert(ofnode_valid(node));
	ut_assertok(abrec_read_priv(node, &priv));
	ut_assertok(vbe_get_blk(priv.storage, &blk));
	ut_asserteq(NVDATA_START_BLK,
		    (priv.area_start + priv.state_offset) / MMC_MAX_BLOCK_LEN);

	/* other tests use this block, so put it back afterwards */
	ut_asserteq(1, blk_read(blk, NVDATA_START_BLK, 1, orig));

	ut_assertok(abrec_read_nvdata(&priv, blk, &state));
	ut_asserteq(TEST_VERNUM, state.fw_vernum);
	ut_assert(!state.try_b);
	ut_asserteq(VBETR_UNKNOWN, state.try_result);

	state.try_b = true;
	state.try_count = 2;
	state.try_result = VBETR_TRYING;
	state.pick = VBEP_B;
	ut_assertok(abrec_write_nvdata(&priv, blk, &state));

	memset(&check, '\0', sizeof(check));
	ut_assertok(abrec_read_nvdata(&priv, blk, &check));
	ut_asserteq(TEST_VERNUM, check.fw_vernum);
	ut_assert(check.try_b);
	ut_assert(!check.recovery);
	ut_asserteq(2, check.try_count);
	ut_asserteq(VBETR_TRYING, check.try_result);
	ut_asserteq(VBEP_B, check.pick);

	ut_asserteq(1, blk_write(blk, NVDATA_START_BLK, 1, orig));
	free((char *)priv.storage);

	return 0;
}
BOOTSTD_TEST(vbe_abrec_test_nvdata, UTF_DM | UTF_SCAN_FDT);

/* bind an abrec bootmeth called @name to the node at @path */
static int bind_abrec_node(struct unit_test_state *uts, const char *path,
			   const char *name, struct udevice **devp,
			   struct udevice **blkp)
{
	struct udevice *bootstd, *dev;
	struct abrec_priv *priv;
	ofnode node;

	node = ofnode_path(path);
	ut_assert(ofnode_valid(node));
	ut_assertok(uclass_first_device_err(UCLASS_BOOTSTD, &bootstd));
	ut_assertok(device_bind(bootstd, DM_DRIVER_GET(vbe_abrec), name, NULL,
				node, &dev));
	ut_assertok(device_probe(dev));
	priv = dev_get_priv(dev);
	ut_assertok(vbe_get_blk(priv->storage, blkp));
	*devp = dev;

	return 0;
}

/*
 * firmware0 in the sandbox devicetree is vbe-simple, but has the properties
 * which abrec needs, so put an abrec bootmeth called "abrec" on it too
 */
static int bind_abrec(struct unit_test_state *uts, struct udevice **devp,
		      struct udevice **blkp)
{
	return bind_abrec_node(uts, "/bootstd/firmware0", "abrec", devp, blkp);
}

/* Check the 'vbe recovery' command */
static int vbe_abrec_test_recovery_cmd(struct unit_test_state *uts)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, orig, MMC_MAX_BLOCK_LEN);
	struct udevice *dev, *dev_ab, *blk;
	struct abrec_state state;
	struct abrec_priv *priv;
	bool changed;

	/* firmware0 has no slot-recovery-offset, so it only has A and B */
	ut_assertok(bind_abrec(uts, &dev_ab, &blk));
	ut_assertok(bind_abrec_node(uts, "/bootstd/firmware2", "abrec2", &dev,
				    &blk));
	priv = dev_get_priv(dev);
	ut_assert(priv->has_recovery);
	ut_assert(!((struct abrec_priv *)dev_get_priv(dev_ab))->has_recovery);

	/* other tests use this block, so put it back afterwards */
	ut_asserteq(1, blk_read(blk, NVDATA_START_BLK, 1, orig));

	/* vbe-simple has no recovery firmware */
	ut_assertok(run_command("vbe select firmware0", 0));
	ut_asserteq(1, run_command("vbe recovery", 0));
	ut_assert_nextline("VBE bootmeth 'firmware0' has no recovery firmware");
	ut_assert_console_end();

	/* nor does an abrec bootmeth which only has A and B */
	ut_assertok(run_command("vbe select abrec", 0));
	ut_assertok(run_command("vbe recovery", 0));
	ut_assert_nextline("Recovery: off");
	ut_asserteq(1, run_command("vbe recovery on", 0));
	ut_assert_nextline("VBE bootmeth 'abrec' has no recovery firmware");
	ut_assert_console_end();

	ut_assertok(run_command("vbe select abrec2", 0));
	ut_assertok(run_command("vbe recovery", 0));
	ut_assert_nextline("Recovery: off");
	ut_assert_console_end();

	/* request recovery, which leaves the rest of the state alone */
	ut_assertok(run_command("vbe recovery on", 0));
	ut_assert_nextline("Recovery: on");
	ut_assert_console_end();
	ut_assertok(abrec_read_nvdata(priv, blk, &state));
	ut_assert(state.recovery);
	ut_asserteq(TEST_VERNUM, state.fw_vernum);
	ut_assert(!state.try_b);

	/* and it is what VPL picks */
	ut_asserteq(VBEP_RECOVERY, abrec_choose(&state, true, &changed));

	ut_assertok(run_command("vbe recovery off", 0));
	ut_assert_nextline("Recovery: off");
	ut_assert_console_end();
	ut_assertok(abrec_read_nvdata(priv, blk, &state));
	ut_assert(!state.recovery);
	ut_asserteq(TEST_VERNUM, state.fw_vernum);

	ut_asserteq(1, run_command("vbe recovery maybe", 0));
	ut_assert_skip_to_linen("vbe recovery [on|off]");
	ut_assert_nextlinen("vbe try [on|off]");
	ut_assert_console_end();

	ut_assertok(run_command("vbe select", 0));
	ut_asserteq(1, blk_write(blk, NVDATA_START_BLK, 1, orig));

	return 0;
}
BOOTSTD_TEST(vbe_abrec_test_recovery_cmd, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Check the 'vbe try' command */
static int vbe_abrec_test_try_cmd(struct unit_test_state *uts)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, orig, MMC_MAX_BLOCK_LEN);
	struct udevice *dev, *blk;
	struct abrec_state state;
	struct abrec_priv *priv;
	bool changed;

	ut_assertok(bind_abrec_node(uts, "/bootstd/firmware2", "abrec2", &dev,
				    &blk));
	priv = dev_get_priv(dev);

	/* other tests use this block, so put it back afterwards */
	ut_asserteq(1, blk_read(blk, NVDATA_START_BLK, 1, orig));

	/* vbe-simple has no B firmware */
	ut_assertok(run_command("vbe select firmware0", 0));
	ut_asserteq(1, run_command("vbe try", 0));
	ut_assert_nextline("VBE bootmeth 'firmware0' has no B firmware");
	ut_assert_console_end();

	ut_assertok(run_command("vbe select abrec2", 0));
	ut_assertok(run_command("vbe try", 0));
	ut_assert_nextline("Try B: no");
	ut_assert_console_end();

	/* trying B clears a request for recovery, which VPL would boot instead */
	ut_assertok(run_command("vbe recovery on", 0));
	ut_assert_nextline("Recovery: on");
	ut_assertok(run_command("vbe try on", 0));
	ut_assert_nextline("Try B: yes, 3 tries left");
	ut_assert_console_end();
	ut_assertok(abrec_read_nvdata(priv, blk, &state));
	ut_assert(state.try_b);
	ut_asserteq(3, state.try_count);
	ut_assert(!state.recovery);
	ut_asserteq(TEST_VERNUM, state.fw_vernum);

	/* so VPL picks B, using up a try */
	ut_asserteq(VBEP_B, abrec_choose(&state, true, &changed));
	ut_asserteq(2, state.try_count);

	ut_assertok(run_command("vbe try off", 0));
	ut_assert_nextline("Try B: no");
	ut_assert_console_end();
	ut_assertok(abrec_read_nvdata(priv, blk, &state));
	ut_assert(!state.try_b);
	ut_asserteq(VBEP_A, abrec_choose(&state, true, &changed));

	ut_asserteq(1, run_command("vbe try maybe", 0));
	ut_assert_skip_to_linen("vbe try [on|off]");
	ut_assert_console_end();

	ut_assertok(run_command("vbe select", 0));
	ut_asserteq(1, blk_write(blk, NVDATA_START_BLK, 1, orig));

	return 0;
}
BOOTSTD_TEST(vbe_abrec_test_try_cmd, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Check that 'vbe info' shows the state */
static int vbe_abrec_test_info(struct unit_test_state *uts)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, orig, MMC_MAX_BLOCK_LEN);
	struct udevice *dev, *blk;
	struct abrec_state state;
	struct abrec_priv *priv;

	ut_assertok(bind_abrec(uts, &dev, &blk));
	priv = dev_get_priv(dev);

	/* other tests use this block, so put it back afterwards */
	ut_asserteq(1, blk_read(blk, NVDATA_START_BLK, 1, orig));

	/* B is being tried, with two tries left */
	ut_assertok(abrec_read_nvdata(priv, blk, &state));
	state.try_b = true;
	state.try_count = 2;
	state.try_result = VBETR_TRYING;
	state.pick = VBEP_B;
	ut_assertok(abrec_write_nvdata(priv, blk, &state));

	ut_assertok(run_command("vbe select abrec", 0));
	ut_assertok(run_command("vbe info", 0));
	ut_assert_nextline("Version: %s", TEST_VERSION);
	ut_assert_nextline("Vernum: %x/%x", TEST_VERNUM >> FWVER_KEY_SHIFT,
			   TEST_VERNUM & FWVER_FW_MASK);
	ut_assert_nextline("Try B: yes, 2 tries left");
	ut_assert_nextline("Last result: trying");
	ut_assert_nextline("Recovery: off");
	ut_assert_nextline("Picked: B");
	ut_assert_console_end();

	ut_assertok(run_command("vbe select", 0));
	ut_asserteq(1, blk_write(blk, NVDATA_START_BLK, 1, orig));

	return 0;
}
BOOTSTD_TEST(vbe_abrec_test_info, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Check that the version comes from the slot info of the booted firmware */
static int vbe_abrec_test_slot_info(struct unit_test_state *uts)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, orig, 0x1000);
	ALLOC_CACHE_ALIGN_BUFFER(u8, info, 0x1000);
	struct udevice *dev, *blk;
	struct abrec_state state;
	struct abrec_priv *priv;
	ulong start, blks;

	ut_assertok(bind_abrec_node(uts, "/bootstd/firmware2", "abrec2", &dev,
				    &blk));
	priv = dev_get_priv(dev);
	ut_asserteq(0x40000, priv->slot_a);
	ut_asserteq(0x1f000, priv->slot_info_offset);
	ut_asserteq(0x1000, priv->slot_info_size);
	ut_asserteq(0, priv->version_size);

	/* other tests may use this part of the MMC, so put it back afterwards */
	start = (priv->slot_a + priv->slot_info_offset) / MMC_MAX_BLOCK_LEN;
	blks = priv->slot_info_size / MMC_MAX_BLOCK_LEN;
	ut_asserteq(blks, blk_read(blk, start, blks, orig));

	/* a slot which has no slot info has no version */
	memset(info, '\0', 0x1000);
	ut_asserteq(blks, blk_write(blk, start, blks, info));
	ut_assertok(abrec_read_state(dev, &state));
	ut_asserteq_str("", state.fw_version);

	/* a devicetree which is not slot info has no version either */
	ut_assertok(fdt_create_empty_tree(info, 0x1000));
	ut_assertok(fdt_setprop_string(info, 0, "version", "2026.10.5"));
	ut_assertok(fdt_setprop_u32(info, 0, "size", 0x1234));
	ut_asserteq(blks, blk_write(blk, start, blks, info));
	ut_assertok(abrec_read_state(dev, &state));
	ut_asserteq_str("", state.fw_version);

	/* nothing booted with VBE here, so A is the firmware in use */
	ut_assertok(fdt_setprop_string(info, 0, "compatible",
				       "fwupd,vbe-slot-info"));
	ut_asserteq(blks, blk_write(blk, start, blks, info));
	ut_assertok(abrec_read_state(dev, &state));
	ut_asserteq_str("2026.10.5", state.fw_version);

	ut_assertok(run_command("vbe select abrec2", 0));
	ut_assertok(run_command("vbe info", 0));
	ut_assert_nextline("Version: 2026.10.5");
	ut_assert_skip_to_linen("Picked: ");
	ut_assert_console_end();

	ut_assertok(run_command("vbe select", 0));
	ut_asserteq(blks, blk_write(blk, start, blks, orig));

	return 0;
}
BOOTSTD_TEST(vbe_abrec_test_slot_info, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Check the information provided to the OS */
static int vbe_abrec_test_fixup(struct unit_test_state *uts)
{
	static const char compat[] = "vendor,board\0vendor,soc";
	struct abrec_state state;
	const char *str;
	ofnode node;
	oftree tree;
	u32 vernum;

	memset(&state, '\0', sizeof(state));
	strlcpy(state.fw_version, TEST_VERSION, sizeof(state.fw_version));
	state.fw_vernum = TEST_VERNUM;

	ut_assertok(oftree_new(&tree));
	ut_assertok(ofnode_add_subnode(oftree_root(tree), "firmware0", &node));
	ut_assertok(abrec_fixup_node(node, &state, VBEP_B));

	ut_asserteq_str(TEST_VERSION, ofnode_read_string(node, "cur-version"));
	ut_assertok(ofnode_read_u32(node, "cur-vernum", &vernum));
	ut_asserteq(TEST_VERNUM, vernum);
	ut_asserteq_str(version_string,
			ofnode_read_string(node, "bootloader-version"));
	ut_asserteq_str("b", ofnode_read_string(node, "booted-slot"));

	/* without a handoff from VPL the slot is not known */
	ut_assertok(ofnode_add_subnode(oftree_root(tree), "firmware1", &node));
	ut_assertok(abrec_fixup_node(node, &state, VBEP_COUNT));
	ut_assertnull(ofnode_read_string(node, "booted-slot"));

	/* an erased version block gives no version */
	memset(state.fw_version, 0xff, sizeof(state.fw_version) - 1);
	ut_assertok(ofnode_add_subnode(oftree_root(tree), "firmware2", &node));
	ut_assertok(abrec_fixup_node(node, &state, VBEP_A));
	ut_assertnull(ofnode_read_string(node, "cur-version"));
	ut_asserteq_str("a", ofnode_read_string(node, "booted-slot"));

	/* fwupd identifies the board from the compatible strings */
	ut_assertok(ofnode_write_prop(oftree_root(tree), "compatible",
				      compat, sizeof(compat), false));
	ut_assertok(ofnode_add_subnode(oftree_root(tree), "fwupd", &node));
	ut_assertok(abrec_add_compatible(tree, node));
	ut_assertok(ofnode_read_string_index(node, "compatible", 0, &str));
	ut_asserteq_str("vendor,board", str);
	ut_assertok(ofnode_read_string_index(node, "compatible", 1, &str));
	ut_asserteq_str("vendor,soc", str);

	oftree_dispose(tree);

	return 0;
}
BOOTSTD_TEST(vbe_abrec_test_fixup, UTF_DM | UTF_SCAN_FDT);
