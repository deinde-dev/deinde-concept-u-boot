// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for vbe-simple bootmeth. All start with 'vbe_simple'
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <blk.h>
#include <bootmeth.h>
#include <dm.h>
#include <image.h>
#include <memalign.h>
#include <mmc.h>
#include <of_live.h>
#include <vbe.h>
#include <test/ut.h>
#include <u-boot/crc.h>
#include "bootstd_common.h"
#include "../../boot/vbe_common.h"

/*
 * Basic test of reading nvdata and updating a fwupd node in the device tree
 *
 * This sets up its own VBE info in the device, using bootstd_setup_for_tests()
 * then does a VBE fixup and checks that everything is present.
 */
static int vbe_simple_test_base(struct unit_test_state *uts)
{
	const char *version, *bl_version;
	struct event_ft_fixup fixup;
	struct udevice *dev;
	struct device_node *np;
	char fdt_buf[0x400];
	char info[100];
	int node_ofs;
	ofnode node;
	u32 vernum;

	/* Read the version back */
	ut_assertok(vbe_find_by_any("firmware0", &dev));
	ut_assertok(bootmeth_get_state_desc(dev, info, sizeof(info)));
	ut_asserteq_str("Version: " TEST_VERSION "\nVernum: 1/2", info);

	ut_assertok(fdt_create_empty_tree(fdt_buf, sizeof(fdt_buf)));
	node_ofs = fdt_add_subnode(fdt_buf, 0, "chosen");
	ut_assert(node_ofs > 0);

	node_ofs = fdt_add_subnode(fdt_buf, node_ofs, "fwupd");
	ut_assert(node_ofs > 0);

	node_ofs = fdt_add_subnode(fdt_buf, node_ofs, "firmware0");
	ut_assert(node_ofs > 0);

	if (of_live_active()) {
		ut_assertok(unflatten_device_tree(fdt_buf, &np));
		fixup.tree = oftree_from_np(np);
	} else {
		fixup.tree = oftree_from_fdt(fdt_buf);
	}

	/*
	 * It would be better to call image_setup_libfdt() here, but that
	 * function does not allow passing an ofnode. We can pass fdt_buf but
	 * when it comes to send the event, it creates an ofnode that uses the
	 * control FDT, since it has no way of accessing the live tree created
	 * here.
	 *
	 * Two fix this we need image_setup_libfdt() is updated to use ofnode
	 */
	fixup.images = NULL;
	ut_assertok(event_notify(EVT_FT_FIXUP, &fixup, sizeof(fixup)));

	node = oftree_path(fixup.tree, "/chosen/fwupd/firmware0");

	version = ofnode_read_string(node, "cur-version");
	ut_assertnonnull(version);
	ut_asserteq_str(TEST_VERSION, version);

	ut_assertok(ofnode_read_u32(node, "cur-vernum", &vernum));
	ut_asserteq(TEST_VERNUM, vernum);

	bl_version = ofnode_read_string(node, "bootloader-version");
	ut_assertnonnull(bl_version);
	ut_asserteq_str(version_string, bl_version);

	return 0;
}
BOOTSTD_TEST(vbe_simple_test_base, UTF_DM | UTF_SCAN_FDT);

/* Check that state whose header does not cover its fields is rejected */
static int vbe_simple_test_nvdata_size(struct unit_test_state *uts)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, orig, MMC_MAX_BLOCK_LEN);
	ALLOC_CACHE_ALIGN_BUFFER(u8, buf, MMC_MAX_BLOCK_LEN);
	struct vbe_nvdata *nvd = (struct vbe_nvdata *)buf;
	u32 area_start, state_offset;
	struct udevice *blk;
	ulong offset, blknum;
	ofnode node;

	node = ofnode_path("/bootstd/firmware0");
	ut_assert(ofnode_valid(node));
	ut_assertok(ofnode_read_u32(node, "area-start", &area_start));
	ut_assertok(ofnode_read_u32(node, "state-offset", &state_offset));
	ut_assertok(vbe_get_blk(ofnode_read_string(node, "storage"), &blk));
	offset = area_start + state_offset;
	blknum = offset / MMC_MAX_BLOCK_LEN;

	/* other tests use this block, so put it back afterwards */
	ut_asserteq(1, blk_read(blk, blknum, 1, orig));

	/*
	 * a size of one byte leaves nothing for the CRC8 to cover, so a CRC of
	 * zero matches, but the flags are not covered
	 */
	memset(buf, '\0', MMC_MAX_BLOCK_LEN);
	nvd->hdr = NVD_HDR_VER_CUR;
	nvd->flags = 7;
	ut_asserteq(1, blk_write(blk, blknum, 1, buf));
	ut_asserteq(-EPERM, vbe_read_nvdata(blk, offset, MMC_MAX_BLOCK_LEN, buf));

	/* 16 bytes is the smallest size which covers them */
	memset(buf, '\0', MMC_MAX_BLOCK_LEN);
	nvd->hdr = NVD_HDR_VER_CUR | 4 << NVD_HDR_SIZE_SHIFT;
	nvd->flags = 7;
	nvd->crc8 = crc8(0, buf + 1, 15);
	ut_asserteq(1, blk_write(blk, blknum, 1, buf));
	ut_assertok(vbe_read_nvdata(blk, offset, MMC_MAX_BLOCK_LEN, buf));

	ut_asserteq(1, blk_write(blk, blknum, 1, orig));

	return 0;
}
BOOTSTD_TEST(vbe_simple_test_nvdata_size, UTF_DM | UTF_SCAN_FDT);
