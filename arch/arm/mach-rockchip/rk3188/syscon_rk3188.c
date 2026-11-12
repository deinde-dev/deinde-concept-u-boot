// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/arch-rockchip/clock.h>
#include <dm/prop.h>

static int rk3188_syscon_bind(struct udevice *dev)
{
	DTPLAT(syscon_rk3188, dt, dev);

	/* With of-platdata, nothing else sets the driver data */
	dev->driver_data = dev_prop_driver_data(dev, dt);

	return 0;
}

/* With a devicetree, the syscon uclass sets up the regmap instead */
static int rk3188_syscon_of_to_plat(struct udevice *dev)
{
	struct syscon_uc_info *priv = dev_get_uclass_priv(dev);
	DTPLAT(syscon_rk3188, dt, dev);

	return dev_prop_regmap(dev, dt, &priv->regmap);
}

static __maybe_unused const struct udevice_id rk3188_syscon_ids[] = {
	{ .compatible = "rockchip,rk3188-noc", .data = ROCKCHIP_SYSCON_NOC },
	{ .compatible = "rockchip,rk3188-grf", .data = ROCKCHIP_SYSCON_GRF },
	{ .compatible = "rockchip,rk3188-pmu", .data = ROCKCHIP_SYSCON_PMU },
	{ }
};

U_BOOT_DRIVER(syscon_rk3188) = {
	.name = "syscon_rk3188",
	.id = UCLASS_SYSCON,
	.of_match = of_match_ptr(rk3188_syscon_ids),
	.bind = rk3188_syscon_bind,
	.of_to_plat = IS_ENABLED(CONFIG_OF_PLATDATA) ?
		rk3188_syscon_of_to_plat : NULL,
	DM_HEADER(<asm/arch-rockchip/clock.h>)
};
