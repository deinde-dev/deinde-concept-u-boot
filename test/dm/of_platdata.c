// SPDX-License-Identifier: GPL-2.0+

#include <dm.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>
#include <asm/global_data.h>

/* Test that we can find a device using of-platdata */
static int dm_test_of_plat_base(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_SERIAL, &dev));
	ut_asserteq_str("sandbox_serial", dev->name);

	return 0;
}
DM_TEST(dm_test_of_plat_base, UTF_SCAN_PDATA);

/*
 * find_driver_info - recursively find the driver_info for a device
 *
 * This sets found[idx] to true when it finds the driver_info record for a
 * device, where idx is the index in the driver_info linker list.
 *
 * @uts: Test state
 * @parent: Parent to search
 * @found: bool array to update
 * Return: 0 if OK, non-zero on error
 */
static int find_driver_info(struct unit_test_state *uts, struct udevice *parent,
			    bool found[])
{
	struct udevice *dev;

	/* If not the root device, find the entry that caused it to be bound */
	if (parent->parent) {
		const int n_ents =
			ll_entry_count(struct driver_info, driver_info);
		int idx = -1;
		int i;

		for (i = 0; i < n_ents; i++) {
			const struct driver_rt *drt = gd_dm_driver_rt() + i;

			if (drt->dev == parent) {
				idx = i;
				found[idx] = true;
				break;
			}
		}

		ut_assert(idx != -1);
	}

	device_foreach_child(dev, parent) {
		int ret;

		ret = find_driver_info(uts, dev, found);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* Check that every device is recorded in its driver_info struct */
static int dm_test_of_plat_dev(struct unit_test_state *uts)
{
	const int n_ents = ll_entry_count(struct driver_info, driver_info);
	bool found[n_ents];
	uint i;

	/* Skip this test if there is no platform data */
	if (!IS_ENABLED(CONFIG_OF_PLATDATA))
		return -EAGAIN;

	/* Record the indexes that are found */
	memset(found, '\0', sizeof(found));
	ut_assertok(find_driver_info(uts, gd->dm_root, found));

	/* Make sure that the driver entries without devices have no ->dev */
	for (i = 0; i < n_ents; i++) {
		const struct driver_rt *drt = gd_dm_driver_rt() + i;
		struct udevice *dev;

		if (found[i]) {
			/* Make sure we can find it */
			ut_assertnonnull(drt->dev);
			ut_assertok(device_get_by_ofplat_idx(i, &dev));
			ut_asserteq_ptr(dev, drt->dev);
		} else {
			ut_assertnull(drt->dev);
			ut_asserteq(-ENOENT, device_get_by_ofplat_idx(i, &dev));
		}
	}

	return 0;
}
DM_TEST(dm_test_of_plat_dev, UTF_SCAN_PDATA);

/* Test that devices are numbered using their aliases */
static int dm_test_of_plat_alias(struct unit_test_state *uts)
{
	struct udevice *dev;

	if (!IS_ENABLED(CONFIG_OF_PLATDATA) || !IS_ENABLED(CONFIG_DM_SEQ_ALIAS))
		return -EAGAIN;

	/* This is the first GPIO device, but its alias is gpio2 */
	ut_assertok(uclass_first_device_err(UCLASS_GPIO, &dev));
	ut_asserteq_str("sandbox_gpio", dev->name);
	ut_asserteq(2, dev_seq(dev));
	ut_asserteq(2, lists_highest_alias_seq(UCLASS_GPIO));

	ut_assertok(uclass_first_device_err(UCLASS_I2C, &dev));
	ut_asserteq(0, dev_seq(dev));
	ut_asserteq(0, lists_highest_alias_seq(UCLASS_I2C));

	/* With no alias, a device is numbered from 0 */
	ut_assertok(uclass_first_device_err(UCLASS_SERIAL, &dev));
	ut_asserteq(0, dev_seq(dev));
	ut_asserteq(-1, lists_highest_alias_seq(UCLASS_SERIAL));

	/* A device bound later is numbered after the highest alias */
	ut_assertok(device_bind(gd->dm_root, DM_DRIVER_GET(sandbox_gpio),
				"extra", NULL, ofnode_null(), &dev));
	ut_asserteq(3, dev_seq(dev));
	ut_assertok(device_unbind(dev));

	return 0;
}
DM_TEST(dm_test_of_plat_alias, UTF_SCAN_PDATA);

#if IS_ENABLED(CONFIG_OF_PLATDATA_PARENT)
/* Test that device parents are correctly set up */
static int dm_test_of_plat_parent(struct unit_test_state *uts)
{
	struct udevice *rtc, *i2c;

	ut_assertok(uclass_first_device_err(UCLASS_RTC, &rtc));
	ut_assertok(uclass_first_device_err(UCLASS_I2C, &i2c));
	ut_asserteq_ptr(i2c, dev_get_parent(rtc));

	return 0;
}
DM_TEST(dm_test_of_plat_parent, UTF_SCAN_PDATA);
#endif

