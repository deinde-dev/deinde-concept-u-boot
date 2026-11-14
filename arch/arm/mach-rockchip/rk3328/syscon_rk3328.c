// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2016 Rockchip Electronics Co., Ltd
 */

#include <dm.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/arch-rockchip/clock.h>
#include <dm/prop.h>

static int rk3328_syscon_bind(struct udevice *dev)
{
	DTPLAT(syscon_rk3328, dt, dev);

	/* With of-platdata, nothing else sets the driver data */
	dev->driver_data = dev_prop_driver_data(dev, dt);

	/* With a devicetree, bind the child devices too */
	if (IS_ENABLED(CONFIG_OF_REAL))
		return dm_scan_fdt_dev(dev);

	return 0;
}

/* With a devicetree, the syscon uclass sets up the regmap instead */
static int rk3328_syscon_of_to_plat(struct udevice *dev)
{
	struct syscon_uc_info *priv = dev_get_uclass_priv(dev);
	DTPLAT(syscon_rk3328, dt, dev);

	return dev_prop_regmap(dev, dt, &priv->regmap);
}

static __maybe_unused const struct udevice_id rk3328_syscon_ids[] = {
	{ .compatible = "rockchip,rk3328-grf", .data = ROCKCHIP_SYSCON_GRF },
	{ }
};

U_BOOT_DRIVER(syscon_rk3328) = {
	.name = "syscon_rk3328",
	.id = UCLASS_SYSCON,
	.of_match = of_match_ptr(rk3328_syscon_ids),
	.bind = rk3328_syscon_bind,
	.of_to_plat = IS_ENABLED(CONFIG_OF_PLATDATA) ?
		rk3328_syscon_of_to_plat : NULL,
	DM_HEADER(<asm/arch-rockchip/clock.h>)
};
