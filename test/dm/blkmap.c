// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023 Addiva Elektronik
 * Author: Tobias Waldekranz <tobias@waldekranz.com>
 */

#include <blk.h>
#include <blkmap.h>
#include <dm.h>
#include <asm/test.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>

#define BLKSZ 0x200

struct mapping {
	int src;
	int cnt;
	int dst;
};

const struct mapping unordered_mapping[] = {
	{ 0, 1, 3 },
	{ 1, 3, 0 },
	{ 4, 2, 6 },
	{ 6, 2, 4 },

	{ 0, 0, 0 }
};

const struct mapping identity_mapping[] = {
	{ 0, 8, 0 },

	{ 0, 0, 0 }
};

static char identity[8 * BLKSZ];
static char unordered[8 * BLKSZ];
static char buffer[8 * BLKSZ];

static void mkblob(void *base, const struct mapping *m)
{
	int nr;

	for (; m->cnt; m++) {
		for (nr = 0; nr < m->cnt; nr++) {
			memset(base + (m->dst + nr) * BLKSZ,
			       m->src + nr, BLKSZ);
		}
	}
}

static int dm_test_blkmap_read(struct unit_test_state *uts)
{
	struct udevice *dev, *blk;
	const struct mapping *m;

	ut_assertok(blkmap_create("rdtest", &dev));
	ut_assertok(blk_get_from_parent(dev, &blk));

	/* Generate an ordered and an unordered pattern in memory */
	mkblob(unordered, unordered_mapping);
	mkblob(identity, identity_mapping);

	/* Create a blkmap that cancels out the disorder */
	for (m = unordered_mapping; m->cnt; m++) {
		ut_assertok(blkmap_map_mem(dev, m->src, m->cnt,
					   unordered + m->dst * BLKSZ));
	}

	/* Read out the data via the blkmap device to another area,
	 * and verify that it matches the ordered pattern.
	 */
	ut_asserteq(8, blk_read(blk, 0, 8, buffer));
	ut_assertok(memcmp(buffer, identity, sizeof(buffer)));

	ut_assertok(blkmap_destroy(dev));
	return 0;
}
DM_TEST(dm_test_blkmap_read, 0);

static int dm_test_blkmap_write(struct unit_test_state *uts)
{
	struct udevice *dev, *blk;
	const struct mapping *m;

	ut_assertok(blkmap_create("wrtest", &dev));
	ut_assertok(blk_get_from_parent(dev, &blk));

	/* Generate an ordered and an unordered pattern in memory */
	mkblob(unordered, unordered_mapping);
	mkblob(identity, identity_mapping);

	/* Create a blkmap that mimics the disorder */
	for (m = unordered_mapping; m->cnt; m++) {
		ut_assertok(blkmap_map_mem(dev, m->src, m->cnt,
					   buffer + m->dst * BLKSZ));
	}

	/* Write the ordered data via the blkmap device to another
	 * area, and verify that the result matches the unordered
	 * pattern.
	 */
	ut_asserteq(8, blk_write(blk, 0, 8, identity));
	ut_assertok(memcmp(buffer, unordered, sizeof(buffer)));

	ut_assertok(blkmap_destroy(dev));
	return 0;
}
DM_TEST(dm_test_blkmap_write, 0);

static int dm_test_blkmap_slicing(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(blkmap_create("slicetest", &dev));

	ut_assertok(blkmap_map_mem(dev, 8, 8, NULL));

	/* Can't overlap on the low end */
	ut_asserteq(-EBUSY, blkmap_map_mem(dev,  4, 5, NULL));
	/* Can't be inside */
	ut_asserteq(-EBUSY, blkmap_map_mem(dev, 10, 2, NULL));
	/* Can't overlap on the high end */
	ut_asserteq(-EBUSY, blkmap_map_mem(dev, 15, 4, NULL));

	/* But we should be able to add slices right before and
	 * after
	 */
	ut_assertok(blkmap_map_mem(dev,  4, 4, NULL));
	ut_assertok(blkmap_map_mem(dev, 16, 4, NULL));

	ut_assertok(blkmap_destroy(dev));
	return 0;
}
DM_TEST(dm_test_blkmap_slicing, 0);

static int dm_test_blkmap_creation(struct unit_test_state *uts)
{
	struct udevice *first, *second;

	ut_assertok(blkmap_create("first", &first));

	/* Can't have two "first"s */
	ut_asserteq(-EBUSY, blkmap_create("first", &second));

	/* But "second" should be fine */
	ut_assertok(blkmap_create("second", &second));

	/* Once "first" is destroyed, we should be able to create it
	 * again
	 */
	ut_assertok(blkmap_destroy(first));
	ut_assertok(blkmap_create("first", &first));

	ut_assertok(blkmap_destroy(first));
	ut_assertok(blkmap_destroy(second));
	return 0;
}
DM_TEST(dm_test_blkmap_creation, 0);
