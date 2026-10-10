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
	ut_asserteq(VBEP_A, abrec_choose(&state, &changed));
	ut_assert(!changed);

	/* an update asks to try B three times */
	state.try_b = true;
	state.try_count = 3;
	ut_asserteq(VBEP_B, abrec_choose(&state, &changed));
	ut_assert(changed);
	ut_asserteq(2, state.try_count);
	ut_asserteq(VBETR_TRYING, state.try_result);
	ut_asserteq(VBEP_B, state.pick);

	/* B fails to reach the OS twice more, using up the tries */
	ut_asserteq(VBEP_B, abrec_choose(&state, &changed));
	ut_asserteq(VBEP_B, abrec_choose(&state, &changed));
	ut_asserteq(0, state.try_count);

	/* so it is marked bad and A is used */
	ut_asserteq(VBEP_A, abrec_choose(&state, &changed));
	ut_assert(changed);
	ut_assert(!state.try_b);
	ut_asserteq(VBETR_BAD, state.try_result);
	ut_asserteq(VBEP_A, state.pick);

	/* after that, A is used without any change */
	ut_asserteq(VBEP_A, abrec_choose(&state, &changed));
	ut_assert(!changed);

	/* recovery overrides everything */
	state.recovery = true;
	state.try_b = true;
	state.try_count = 1;
	ut_asserteq(VBEP_RECOVERY, abrec_choose(&state, &changed));
	ut_asserteq(1, state.try_count);

	return 0;
}
BOOTSTD_TEST(vbe_abrec_test_choose, 0);

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
