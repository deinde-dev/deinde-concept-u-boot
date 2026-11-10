// SPDX-License-Identifier: GPL-2.0+
/*
 * Sandbox driver for testing the dev_prop_...() helpers
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
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

	return 0;
}

static const struct udevice_id sandbox_dev_prop_ids[] = {
	{ .compatible = "sandbox,dev-prop-test", },
	{}
};

U_BOOT_DRIVER(sandbox_dev_prop_test) = {
	.name	= "sandbox_dev_prop_test",
	.id	= UCLASS_MISC,
	.of_match = sandbox_dev_prop_ids,
	.of_to_plat = sandbox_dev_prop_of_to_plat,
	.plat_auto = sizeof(struct sandbox_dev_prop_plat),
	.flags	= DM_FLAG_PRE_RELOC,
};
