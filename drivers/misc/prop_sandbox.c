// SPDX-License-Identifier: GPL-2.0+
/*
 * Sandbox driver for testing the dev_prop_...() helpers
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <regmap.h>
#include <asm/test.h>
#include <dm/prop.h>

static int sandbox_dev_prop_of_to_plat(struct udevice *dev)
{
	struct sandbox_dev_prop_plat *plat = dev_get_plat(dev);
	DTPLAT(sandbox_dev_prop_test, dt, dev);
	u32 none[2];

	plat->base = dev_prop_addr(dev, dt);
	plat->present = dev_prop_u32_default(dev, dt, dev_prop_present,
					     "dev-prop-present", 7);
	plat->absent = dev_prop_u32_default(dev, dt, dev_prop_absent,
					    "dev-prop-absent", 9);
	plat->flag = dev_prop_bool(dev, dt, dev_prop_flag, "dev-prop-flag");
	plat->noflag = dev_prop_bool(dev, dt, dev_prop_noflag, "dev-prop-noflag");
	plat->array_ret = dev_prop_u32_array(dev, dt, dev_prop_array,
					     "dev-prop-array", plat->array,
					     ARRAY_SIZE(plat->array));
	plat->missing_array_ret = dev_prop_u32_array(dev, dt, dev_prop_none,
						     "dev-prop-none", none,
						     ARRAY_SIZE(none));

	/* sandbox_vpl has no regmap support in TPL and VPL */
	if (!IS_ENABLED(CONFIG_REGMAP))
		return 0;

	return dev_prop_regmap(dev, dt, &plat->map);
}

/* With of-platdata, nothing else sets the driver data */
static int sandbox_dev_prop_bind(struct udevice *dev)
{
	DTPLAT(sandbox_dev_prop_test, dt, dev);

	/*
	 * Sandbox SPL also binds devices from the devicetree for some tests.
	 * These have no generated data, but driver model sets their driver data
	 */
	if (IS_ENABLED(CONFIG_OF_PLATDATA) && !dev_get_dtplat(dev))
		return 0;
	dev->driver_data = dev_prop_driver_data(dev, dt);

	return 0;
}

static int sandbox_dev_prop_remove(struct udevice *dev)
{
	struct sandbox_dev_prop_plat *plat = dev_get_plat(dev);

	return IS_ENABLED(CONFIG_REGMAP) ? regmap_uninit(plat->map) : 0;
}

static const struct udevice_id sandbox_dev_prop_ids[] = {
	{ .compatible = "sandbox,dev-prop-test", .data = SANDBOX_DEV_PROP_TEST },
	{ .compatible = "sandbox,dev-prop-other", .data = SANDBOX_DEV_PROP_OTHER },
	{}
};

U_BOOT_DRIVER(sandbox_dev_prop_test) = {
	.name	= "sandbox_dev_prop_test",
	.id	= UCLASS_MISC,
	.of_match = sandbox_dev_prop_ids,
	.bind	= sandbox_dev_prop_bind,
	.of_to_plat = sandbox_dev_prop_of_to_plat,
	.remove	= sandbox_dev_prop_remove,
	.plat_auto = sizeof(struct sandbox_dev_prop_plat),
	.flags	= DM_FLAG_PRE_RELOC,
	DM_HEADER(<asm/test.h>)
};

/*
 * A driver with just one node, so that with of-platdata its data is visible to
 * the compiler, rather than being found from the device
 */
static int sandbox_dev_prop_single_of_to_plat(struct udevice *dev)
{
	struct sandbox_dev_prop_plat *plat = dev_get_plat(dev);
	DTPLAT(sandbox_dev_prop_single, dt, dev);

	plat->base = dev_prop_addr(dev, dt);
	plat->present = dev_prop_u32_default(dev, dt, dev_prop_present,
					     "dev-prop-present", 7);
	plat->absent = dev_prop_u32_default(dev, dt, dev_prop_absent,
					    "dev-prop-absent", 9);

	if (!IS_ENABLED(CONFIG_REGMAP))
		return 0;

	return dev_prop_regmap(dev, dt, &plat->map);
}

static const struct udevice_id sandbox_dev_prop_single_ids[] = {
	{ .compatible = "sandbox,dev-prop-single", },
	{}
};

U_BOOT_DRIVER(sandbox_dev_prop_single) = {
	.name	= "sandbox_dev_prop_single",
	.id	= UCLASS_MISC,
	.of_match = sandbox_dev_prop_single_ids,
	.of_to_plat = sandbox_dev_prop_single_of_to_plat,
	.remove	= sandbox_dev_prop_remove,
	.plat_auto = sizeof(struct sandbox_dev_prop_plat),
	.flags	= DM_FLAG_PRE_RELOC,
};
