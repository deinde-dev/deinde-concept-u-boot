// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 Intel Corporation.
 * Copyright 2019 Google LLC
 *
 * Modified from coreboot pmclib.c, pmc.c and pmutil.c
 */

#define LOG_CATEGORY UCLASS_ACPI_PMC

#include <dm.h>
#include <log.h>
#include <spl.h>
#include <acpi/acpi_s3.h>
#include <asm/io.h>
#include <asm/pci.h>
#include <asm/arch/pmc.h>
#include <dm/pci.h>
#include <dm/prop.h>
#include <linux/bitops.h>
#include <power/acpi_pmc.h>

#define GPIO_GPE_CFG		0x1050

/* Memory mapped IO registers behind PMC_BASE_ADDRESS */
#define PRSTS			0x1000
#define GEN_PMCON1		0x1020
#define  COLD_BOOT_STS		BIT(27)
#define  COLD_RESET_STS		BIT(26)
#define  WARM_RESET_STS		BIT(25)
#define  GLOBAL_RESET_STS	BIT(24)
#define  SRS			BIT(20)
#define  MS4V			BIT(18)
#define  RPS			BIT(2)
#define GEN_PMCON1_CLR1_BITS	(COLD_BOOT_STS | COLD_RESET_STS | \
				 WARM_RESET_STS | GLOBAL_RESET_STS | \
				 SRS | MS4V)
#define GEN_PMCON2		0x1024
#define GEN_PMCON3		0x1028

/* Offset of TCO registers from ACPI base I/O address */
#define TCO_REG_OFFSET		0x60
#define TCO1_STS	0x64
#define   DMISCI_STS	BIT(9)
#define   BOOT_STS	BIT(18)
#define TCO2_STS	0x66
#define TCO1_CNT	0x68
#define   TCO_LOCK	BIT(12)
#define TCO2_CNT	0x6a

enum {
	ETR		= 0x1048,
	CF9_LOCK        = 1UL << 31,
	CF9_GLB_RST	= 1 << 20,
};

static int apl_pmc_fill_power_state(struct udevice *dev)
{
	struct acpi_pmc_upriv *upriv = dev_get_uclass_priv(dev);

	upriv->tco1_sts = inw(upriv->acpi_base + TCO1_STS);
	upriv->tco2_sts = inw(upriv->acpi_base + TCO2_STS);

	upriv->prsts = readl(upriv->pmc_bar0 + PRSTS);
	upriv->gen_pmcon1 = readl(upriv->pmc_bar0 + GEN_PMCON1);
	upriv->gen_pmcon2 = readl(upriv->pmc_bar0 + GEN_PMCON2);
	upriv->gen_pmcon3 = readl(upriv->pmc_bar0 + GEN_PMCON3);

	return 0;
}

static int apl_prev_sleep_state(struct udevice *dev, int prev_sleep_state)
{
	struct acpi_pmc_upriv *upriv = dev_get_uclass_priv(dev);

	/* WAK_STS bit will not be set when waking from G3 state */
	if (!(upriv->pm1_sts & WAK_STS) &&
	    (upriv->gen_pmcon1 & COLD_BOOT_STS))
		prev_sleep_state = ACPI_S5;

	return prev_sleep_state;
}

static int apl_disable_tco(struct udevice *dev)
{
	struct acpi_pmc_upriv *upriv = dev_get_uclass_priv(dev);

	pmc_disable_tco_base(upriv->acpi_base + TCO_REG_OFFSET);

	return 0;
}

static int apl_global_reset_set_enable(struct udevice *dev, bool enable)
{
	struct acpi_pmc_upriv *upriv = dev_get_uclass_priv(dev);

	if (enable)
		setbits_le32(upriv->pmc_bar0 + ETR, CF9_GLB_RST);
	else
		clrbits_le32(upriv->pmc_bar0 + ETR, CF9_GLB_RST);

	return 0;
}

int apl_pmc_ofdata_to_uc_plat(struct udevice *dev)
{
	struct acpi_pmc_upriv *upriv = dev_get_uclass_priv(dev);
	struct apl_pmc_plat *plat = dev_get_plat(dev);
	DTPLAT(intel_apl_pmc, dt, dev);
	u32 base[6];
	int size;
	int ret;

	ret = dev_prop_u32_array(dev, dt, early_regs, "early-regs", base,
				 ARRAY_SIZE(base));
	if (ret)
		return log_msg_ret("Missing/short early-regs", ret);
	if (xpl_phase() == PHASE_TPL) {
		upriv->pmc_bar0 = (void *)base[0];
		upriv->pmc_bar2 = (void *)base[2];
		upriv->gpe_cfg = (u32 *)(upriv->pmc_bar0 + GPIO_GPE_CFG);

		/* Since PCI is not enabled, we must get the BDF manually */
		plat->bdf = dev_prop_pci_devfn(dev, dt);
		if (plat->bdf < 0)
			return log_msg_ret("Cannot get PMC PCI address",
					   plat->bdf);
	}
	upriv->acpi_base = base[4];

	/* Get the dwX values for pmc gpe settings */
	size = dev_prop_size(dev, dt, gpe0_dw, "gpe0-dw");
	if (size < 0)
		return log_msg_ret("Cannot read gpe0-dm", size);
	upriv->gpe0_count = min_t(int, size / sizeof(u32), GPE0_REG_MAX);
	ret = dev_prop_u32_array(dev, dt, gpe0_dw, "gpe0-dw", upriv->gpe0_dw,
				 upriv->gpe0_count);
	if (ret)
		return log_msg_ret("Bad gpe0-dw", ret);

	ret = dev_prop_u32(dev, dt, gpe0_dwx_mask, "gpe0-dwx-mask",
			   &upriv->gpe0_dwx_mask);
	if (ret)
		return log_msg_ret("no gpe0-dwx-mask", ret);
	ret = dev_prop_u32(dev, dt, gpe0_dwx_shift_base, "gpe0-dwx-shift-base",
			   &upriv->gpe0_dwx_shift_base);
	if (ret)
		return log_msg_ret("no gpe0-dwx-shift-base", ret);
	ret = dev_prop_u32(dev, dt, gpe0_sts, "gpe0-sts", &upriv->gpe0_sts_reg);
	if (ret)
		return log_msg_ret("no gpe0-sts", ret);
	upriv->gpe0_sts_reg += upriv->acpi_base;
	ret = dev_prop_u32(dev, dt, gpe0_en, "gpe0-en", &upriv->gpe0_en_reg);
	if (ret)
		return log_msg_ret("no gpe0-en", ret);
	upriv->gpe0_en_reg += upriv->acpi_base;

	return 0;
}

static int enable_pmcbar(struct udevice *dev)
{
	struct acpi_pmc_upriv *upriv = dev_get_uclass_priv(dev);
	struct apl_pmc_plat *priv = dev_get_plat(dev);
	pci_dev_t pmc = priv->bdf;

	/*
	 * Set PMC base addresses and enable decoding. BARs 1 and 3 are 64-bit
	 * BARs.
	 */
	pci_x86_write_config(pmc, PCI_BASE_ADDRESS_0, (ulong)upriv->pmc_bar0,
			     PCI_SIZE_32);
	pci_x86_write_config(pmc, PCI_BASE_ADDRESS_1, 0, PCI_SIZE_32);
	pci_x86_write_config(pmc, PCI_BASE_ADDRESS_2, (ulong)upriv->pmc_bar2,
			     PCI_SIZE_32);
	pci_x86_write_config(pmc, PCI_BASE_ADDRESS_3, 0, PCI_SIZE_32);
	pci_x86_write_config(pmc, PCI_BASE_ADDRESS_4, upriv->acpi_base,
			     PCI_SIZE_16);
	pci_x86_write_config(pmc, PCI_COMMAND, PCI_COMMAND_IO |
			     PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER,
			     PCI_SIZE_16);

	return 0;
}

static int apl_pmc_probe(struct udevice *dev)
{
	if (xpl_phase() == PHASE_TPL) {
		return enable_pmcbar(dev);
	} else {
		struct acpi_pmc_upriv *upriv = dev_get_uclass_priv(dev);

		upriv->pmc_bar0 = (void *)dm_pci_read_bar32(dev, 0);
		upriv->pmc_bar2 = (void *)dm_pci_read_bar32(dev, 2);
	}

	return 0;
}

static const struct acpi_pmc_ops apl_pmc_ops = {
	.init			= apl_pmc_fill_power_state,
	.prev_sleep_state	= apl_prev_sleep_state,
	.disable_tco		= apl_disable_tco,
	.global_reset_set_enable = apl_global_reset_set_enable,
};

static __maybe_unused const struct udevice_id apl_pmc_ids[] = {
	{ .compatible = "intel,apl-pmc" },
	{ }
};

U_BOOT_DRIVER(intel_apl_pmc) = {
	.name		= "intel_apl_pmc",
	.id		= UCLASS_ACPI_PMC,
	.of_match	= of_match_ptr(apl_pmc_ids),
	.of_to_plat = apl_pmc_ofdata_to_uc_plat,
	.probe		= apl_pmc_probe,
	.ops		= &apl_pmc_ops,
	.plat_auto	= sizeof(struct apl_pmc_plat),
};
