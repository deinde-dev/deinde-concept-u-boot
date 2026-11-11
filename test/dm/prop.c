// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the dev_prop_...() helpers, which read the same properties with or
 * without of-platdata
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <errno.h>
#include <asm/global_data.h>
#include <asm/test.h>
#include <dm/device-internal.h>
#include <dm/prop.h>
#include <dm/root.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/**
 * get_dev() - Find and probe the device for a driver at a given address
 *
 * @uts: Test state
 * @drv: Driver to look for
 * @base: Address from the device's 'reg' property
 * @devp: Returns the device
 * Return: 0 if OK, -ENOENT if not found
 */
static int get_dev(struct unit_test_state *uts, struct driver *drv, ulong base,
		   struct udevice **devp)
{
	struct udevice *dev;
	struct uclass *uc;

	uclass_id_foreach_dev(UCLASS_MISC, dev, uc) {
		struct sandbox_dev_prop_plat *plat;

		if (dev->driver != drv)
			continue;
		ut_assertok(device_probe(dev));
		plat = dev_get_plat(dev);
		if (plat->base == base) {
			*devp = dev;
			return 0;
		}
	}

	return -ENOENT;
}

/* The same values must be read from the devicetree and from of-platdata */
static int dm_test_dev_prop(struct unit_test_state *uts)
{
	struct sandbox_dev_prop_plat *plat;
	struct udevice *dev;

	ut_assertok(get_dev(uts, DM_DRIVER_GET(sandbox_dev_prop_test),
			    0x12340000, &dev));
	plat = dev_get_plat(dev);
	ut_asserteq(0x12340000, plat->base);
	ut_asserteq(42, plat->present);
	ut_asserteq(9, plat->absent);
	ut_assert(plat->flag);
	ut_assert(!plat->noflag);
	ut_assertok(plat->array_ret);
	ut_asserteq(1, plat->array[0]);
	ut_asserteq(2, plat->array[1]);
	ut_asserteq(3, plat->array[2]);
	ut_asserteq(-EINVAL, plat->missing_array_ret);

	/* Two nodes use the driver, so the data is found from the device */
	if (IS_ENABLED(CONFIG_OF_PLATDATA))
		ut_assertnonnull(dev_get_dtplat(dev));

	/* The second node has most properties absent */
	ut_assertok(get_dev(uts, DM_DRIVER_GET(sandbox_dev_prop_test),
			    0x12350000, &dev));
	plat = dev_get_plat(dev);
	ut_asserteq(7, plat->present);
	ut_asserteq(9, plat->absent);
	ut_assert(!plat->flag);
	ut_asserteq(-EINVAL, plat->array_ret);

	return 0;
}
DM_TEST(dm_test_dev_prop, IS_ENABLED(CONFIG_OF_PLATDATA) ? UTF_SCAN_PDATA :
	UTF_SCAN_FDT);

/* Check a driver with just one node, whose data is visible to the compiler */
static int dm_test_dev_prop_single(struct unit_test_state *uts)
{
	struct sandbox_dev_prop_plat *plat;
	struct udevice *dev;

	ut_assertok(uclass_get_device_by_driver(UCLASS_MISC,
				DM_DRIVER_GET(sandbox_dev_prop_single), &dev));
	plat = dev_get_plat(dev);
	ut_asserteq(0x12360000, plat->base);
	ut_asserteq(5, plat->present);
	ut_asserteq(9, plat->absent);

	/* With of-platdata, the device does not point to the data */
	if (IS_ENABLED(CONFIG_OF_PLATDATA))
		ut_assertnull(dev_get_dtplat(dev));

	return 0;
}
DM_TEST(dm_test_dev_prop_single, IS_ENABLED(CONFIG_OF_PLATDATA) ?
	UTF_SCAN_PDATA : UTF_SCAN_FDT);

/* A device bound by C code must not use the data for a driver's only node */
static int dm_test_dev_prop_single_bind(struct unit_test_state *uts)
{
	struct driver_rt *drt = gd_dm_driver_rt();
	struct udevice *dev, *extra;
	int count, idx;

	/* This needs of-platdata devices to be bound at run time */
	if (!IS_ENABLED(CONFIG_OF_PLATDATA_CHECK))
		return -EAGAIN;

	ut_assertok(uclass_get_device_by_driver(UCLASS_MISC,
				DM_DRIVER_GET(sandbox_dev_prop_single), &dev));

	/* Find the record which the device was bound from */
	count = ll_entry_count(struct driver_info, driver_info);
	for (idx = 0; idx < count && drt[idx].dev != dev; idx++)
		;
	ut_assert(idx < count);
	ut_assertok(dtplat_check_single(dev, idx));

	ut_assertok(device_bind(dm_root(),
				DM_DRIVER_GET(sandbox_dev_prop_single),
				"extra", NULL, ofnode_null(), &extra));
	ut_asserteq(-EINVAL, dtplat_check_single(extra, idx));
	ut_assertok(device_unbind(extra));

	return 0;
}
DM_TEST(dm_test_dev_prop_single_bind, IS_ENABLED(CONFIG_OF_PLATDATA) ?
	UTF_SCAN_PDATA : UTF_SCAN_FDT);
