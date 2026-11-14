// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2019 Rockchip Electronics Co., Ltd
 * Copyright (c) 2022 Edgeble AI Technologies Pvt. Ltd.
 */

#include <dm.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/arch-rockchip/clock.h>
#include <dm/prop.h>

static int rv1126_syscon_bind(struct udevice *dev)
{
	DTPLAT(syscon_rv1126, dt, dev);

	/* With of-platdata, nothing else sets the driver data */
	dev->driver_data = dev_prop_driver_data(dev, dt);

	return 0;
}

/* With a devicetree, the syscon uclass sets up the regmap instead */
static int rv1126_syscon_of_to_plat(struct udevice *dev)
{
	struct syscon_uc_info *priv = dev_get_uclass_priv(dev);
	DTPLAT(syscon_rv1126, dt, dev);

	return dev_prop_regmap(dev, dt, &priv->regmap);
}

static __maybe_unused const struct udevice_id rv1126_syscon_ids[] = {
	{ .compatible = "rockchip,rv1126-grf", .data = ROCKCHIP_SYSCON_GRF },
	{ .compatible = "rockchip,rv1126-pmugrf", .data = ROCKCHIP_SYSCON_PMUGRF },
	{ }
};

U_BOOT_DRIVER(syscon_rv1126) = {
	.name = "syscon_rv1126",
	.id = UCLASS_SYSCON,
	.of_match = of_match_ptr(rv1126_syscon_ids),
	.bind = rv1126_syscon_bind,
	.of_to_plat = IS_ENABLED(CONFIG_OF_PLATDATA) ?
		rv1126_syscon_of_to_plat : NULL,
	DM_HEADER(<asm/arch-rockchip/clock.h>)
};
