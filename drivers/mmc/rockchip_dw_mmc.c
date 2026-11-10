// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2013 Google, Inc
 */

#include <bloblist.h>
#include <clk.h>
#include <dm.h>
#include <dwmmc.h>
#include <errno.h>
#include <log.h>
#include <mapmem.h>
#include <pwrseq.h>
#include <spl.h>
#include <syscon.h>
#include <asm/gpio.h>
#include <asm/arch-rockchip/clock.h>
#include <asm/arch-rockchip/periph.h>
#include <dm/prop.h>
#include <linux/delay.h>
#include <linux/err.h>

struct rockchip_mmc_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

struct rockchip_dwmmc_priv {
	struct clk clk;
	struct dwmci_host host;
	int fifo_depth;
	bool fifo_mode;
	u32 minmax[2];
};

static uint rockchip_dwmmc_get_mmc_clk(struct dwmci_host *host, uint freq)
{
	struct udevice *dev = host->priv;
	struct rockchip_dwmmc_priv *priv = dev_get_priv(dev);
	int ret;

	/*
	 * The clock frequency chosen here affects CLKDIV in the dw_mmc core.
	 * That can be either 0 or 1, but it must be set to 1 for eMMC DDR52
	 * 8-bit mode.  It will be set to 0 for all other modes.
	 */
	if (host->mmc->selected_mode == MMC_DDR_52 && host->mmc->bus_width == 8)
		freq *= 2;

	ret = clk_set_rate(&priv->clk, freq);
	if (ret < 0) {
		/*
		 * If CLK is not available (e.g. VPL), use the rate saved
		 * by the previous phase via bloblist. The CRU is still
		 * configured from that phase, so the DW MMC core can
		 * calculate the correct clock divider.
		 */
		if (!IS_ENABLED(CONFIG_CLK) &&
		    IS_ENABLED(CONFIG_MMC_DW_ROCKCHIP_CLK_HANDOFF)) {
			u32 *ratep;

			ratep = bloblist_find(BLOBLISTT_U_BOOT_MMC_CLK,
					      sizeof(*ratep));
			if (ratep)
				return *ratep;
		}
		debug("%s: err=%d\n", __func__, ret);
		return freq;
	}

	/* Save the source clock rate for the next phase */
	if (IS_ENABLED(CONFIG_XPL_BUILD) &&
	    IS_ENABLED(CONFIG_MMC_DW_ROCKCHIP_CLK_HANDOFF)) {
		u32 *ratep;

		ratep = bloblist_ensure(BLOBLISTT_U_BOOT_MMC_CLK,
					sizeof(*ratep));
		if (ratep) {
			ulong rate = clk_get_rate(&priv->clk);

			if (!IS_ERR_VALUE(rate))
				*ratep = rate;
		}
	}

	return freq;
}

static int rockchip_dwmmc_of_to_plat(struct udevice *dev)
{
	struct rockchip_dwmmc_priv *priv = dev_get_priv(dev);
	struct dwmci_host *host = &priv->host;
	DTPLAT(rockchip_rk3288_dw_mshc, dt, dev);

	host->name = dev->name;
	host->ioaddr = dev_prop_addr_ptr(dev, dt);
	host->buswidth = dev_prop_u32_default(dev, dt, bus_width, "bus-width", 4);
	host->get_mmc_clk = rockchip_dwmmc_get_mmc_clk;
	host->priv = dev;

	/* use non-removeable as sdcard and emmc as judgement */
	if (dev_prop_bool(dev, dt, non_removable, "non-removable"))
		host->dev_index = 0;
	else
		host->dev_index = 1;

	priv->fifo_depth = dev_prop_u32_default(dev, dt, fifo_depth,
						"fifo-depth", 0);

	if (priv->fifo_depth < 0)
		return log_msg_ret("rkp", -EINVAL);
	priv->fifo_mode = dev_prop_bool(dev, dt, fifo_mode, "fifo-mode");

	if (is_xpl() && !priv->fifo_mode)
		priv->fifo_mode = dev_prop_bool(dev, dt, u_boot_spl_fifo_mode,
						"u-boot,spl-fifo-mode");

	/*
	 * 'clock-freq-min-max' is deprecated
	 * (see https://github.com/torvalds/linux/commit/b023030f10573de738bbe8df63d43acab64c9f7b)
	 */
	if (dev_prop_u32_array(dev, dt, clock_freq_min_max,
			       "clock-freq-min-max", priv->minmax, 2)) {
		int val = dev_prop_u32_default(dev, dt, max_frequency,
					       "max-frequency", -EINVAL);

		if (val < 0)
			return log_msg_ret("rkc", val);

		priv->minmax[0] = 400000;  /* 400 kHz */
		priv->minmax[1] = val;
	} else {
		debug("%s: 'clock-freq-min-max' property was deprecated.\n",
		      __func__);
	}

	return 0;
}

static int rockchip_dwmmc_probe(struct udevice *dev)
{
	struct rockchip_mmc_plat *plat = dev_get_plat(dev);
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct rockchip_dwmmc_priv *priv = dev_get_priv(dev);
	struct dwmci_host *host = &priv->host;
	DTPLAT(rockchip_rk3288_dw_mshc, dt, dev);
	int ret;

	ret = dev_prop_clk(dev, dt, clocks, "clocks", 1, &priv->clk);
	if (ret < 0 && ret != -ENOSYS)
		return log_msg_ret("clk", ret);
	host->fifo_depth = priv->fifo_depth;
	host->fifo_mode = priv->fifo_mode;

#if IS_ENABLED(CONFIG_MMC_PWRSEQ)
	/* Enable power if needed */
	ret = mmc_pwrseq_get_power(dev, &plat->cfg);
	if (!ret) {
		ret = pwrseq_set_power(plat->cfg.pwr_dev, true);
		if (ret)
			return ret;
	}
#endif
	dwmci_setup_cfg(&plat->cfg, host, priv->minmax[1], priv->minmax[0]);
	host->mmc = &plat->mmc;
	host->mmc->priv = &priv->host;
	host->mmc->dev = dev;
	upriv->mmc = host->mmc;

	/* Hosts capable of 8-bit can also do 4 bits */
	if (host->buswidth == 8)
		plat->cfg.host_caps |= MMC_MODE_4BIT;

	return dwmci_probe(dev);
}

static int rockchip_dwmmc_bind(struct udevice *dev)
{
	struct rockchip_mmc_plat *plat = dev_get_plat(dev);

	return dwmci_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id rockchip_dwmmc_ids[] = {
	{ .compatible = "rockchip,rk2928-dw-mshc" },
	{ .compatible = "rockchip,rk3288-dw-mshc" },
	{ .compatible = "rockchip,rk3576-dw-mshc" },
	{ }
};

U_BOOT_DRIVER(rockchip_rk3288_dw_mshc) = {
	.name		= "rockchip_rk3288_dw_mshc",
	.id		= UCLASS_MMC,
	.of_match	= rockchip_dwmmc_ids,
	.of_to_plat = rockchip_dwmmc_of_to_plat,
	.ops		= &dm_dwmci_ops,
	.bind		= rockchip_dwmmc_bind,
	.probe		= rockchip_dwmmc_probe,
	.priv_auto	= sizeof(struct rockchip_dwmmc_priv),
	.plat_auto	= sizeof(struct rockchip_mmc_plat),
};

DM_DRIVER_ALIAS(rockchip_rk3288_dw_mshc, rockchip_rk2928_dw_mshc)
DM_DRIVER_ALIAS(rockchip_rk3288_dw_mshc, rockchip_rk3328_dw_mshc)
DM_DRIVER_ALIAS(rockchip_rk3288_dw_mshc, rockchip_rk3368_dw_mshc)
