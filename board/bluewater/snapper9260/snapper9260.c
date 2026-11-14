// SPDX-License-Identifier: GPL-2.0+
/*
 * Bluewater Systems Snapper 9260/9G20 modules
 *
 * (C) Copyright 2011 Bluewater Systems
 *   Author: Andre Renaud <andre@bluewatersys.com>
 *   Author: Ryan Mallon <ryan@bluewatersys.com>
 */

#include <config.h>
#include <debug_uart.h>
#include <dm.h>
#include <fdtdec.h>
#include <init.h>
#include <log.h>
#include <asm/global_data.h>
#include <asm/mach-types.h>
#include <asm/arch/at91_common.h>
#include <dm/pinctrl.h>
#include <dm/uclass-internal.h>
#include <linux/delay.h>
#include <power/regulator.h>

DECLARE_GLOBAL_DATA_PTR;

/**
 * eth_phy_init() - Power-cycle and reset the Ethernet PHY
 *
 * The PHY reads its configuration from some of the RMII pins as it comes out
 * of reset. The pull-ups which the SoC enables on its pins at reset put the
 * PHY in a test mode, so select the pin configuration for the Ethernet
 * device first, since it has no pull-ups.
 *
 * The power to the PHY comes from a regulator which is controlled by an I/O
 * expander on the I2C bus. If that cannot be used, the PHY is just reset.
 *
 * Return: 0 if OK, -ve if there is no Ethernet device
 */
static int eth_phy_init(void)
{
	struct udevice *dev, *reg;
	int ret;

	/*
	 * Find the device without probing it, since only its pins are needed
	 * here and probing it would start up the MAC
	 */
	ret = uclass_find_first_device(UCLASS_ETH, &dev);
	if (!ret && !dev)
		ret = -ENODEV;
	if (ret)
		return log_msg_ret("eth", ret);
	ret = pinctrl_select_state(dev, "default");
	if (ret)
		log_warning("Cannot set up Ethernet pins (err=%dE)\n", ret);

	ret = device_get_supply_regulator(dev, "phy-supply", &reg);
	if (ret) {
		log_warning("No Ethernet power control (err=%dE)\n", ret);
	} else {
		regulator_set_enable(reg, false);
		regulator_set_enable(reg, true);
	}

	at91_phy_reset();

	/* The PHY's internal reset takes 21ms */
	mdelay(21);

	return 0;
}

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
void board_debug_uart_init(void)
{
	at91_seriald_hw_init();
}
#endif

int board_init(void)
{
	/* The mach-type is the same for both Snapper 9260 and 9G20 */
	gd->bd->bi_arch_number = MACH_TYPE_SNAPPER_9260;

	/* Address of boot parameters */
	gd->bd->bi_boot_params = gd->ram_base + 0x100;

	/* Ethernet is not essential, so carry on if this fails */
	eth_phy_init();

	return 0;
}

int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}
