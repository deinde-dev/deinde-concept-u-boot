// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the dev_prop_...() helpers, which read the same properties with or
 * without of-platdata
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <errno.h>
#include <asm/test.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>

/* The same values must be read from the devicetree and from of-platdata */
static int dm_test_dev_prop(struct unit_test_state *uts)
{
	struct sandbox_dev_prop_plat *plat;
	struct udevice *dev;

	ut_assertok(uclass_get_device_by_driver(UCLASS_MISC,
				DM_DRIVER_GET(sandbox_dev_prop_test), &dev));
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

	return 0;
}
DM_TEST(dm_test_dev_prop, IS_ENABLED(CONFIG_OF_PLATDATA) ? UTF_SCAN_PDATA :
	UTF_SCAN_FDT);
