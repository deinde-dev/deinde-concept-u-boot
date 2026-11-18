// SPDX-License-Identifier: GPL-2.0+
/*
 * SPL set-up for Firefly-RK3288
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <init.h>
#include <asm/io.h>
#include <asm/arch-rockchip/grf_rk3288.h>
#include <asm/arch-rockchip/hardware.h>

#define GRF_BASE	0xff770000

/* The SD pins are GPIO6 C0-C6: four data lines, clock, command and detect */
enum {
	/* Where group C of GPIO6 is in the pull and drive-strength registers */
	SD_BANK		= 5,
	SD_GROUP	= 2,

	/* There are two bits for each pin: 1 is pull-up and 3 is 12mA */
	SD_PULL_MASK	= 0x3fff,
	SD_PULL		= 0x1455,	/* pull up all but the clock, C4 */
	SD_DRV_MASK	= 0xfff,	/* all but detect, C6 */
	SD_DRV_12MA	= 0xfff,
};

/*
 * SPL has no pin controller, so set up the pins for the SD card here, as the
 * devicetree does. The default drive strength is not enough for the card to
 * work in high-speed mode on this board, so 12mA is needed.
 *
 * The eMMC needs nothing, since the BootROM sets it up before it looks at the
 * SD card
 */
int board_early_init_f(void)
{
	struct rk3288_grf *grf = (void *)GRF_BASE;

	rk_clrsetreg(&grf->gpio6c_iomux,
		     GPIO6C6_MASK << GPIO6C6_SHIFT |
		     GPIO6C5_MASK << GPIO6C5_SHIFT |
		     GPIO6C4_MASK << GPIO6C4_SHIFT |
		     GPIO6C3_MASK << GPIO6C3_SHIFT |
		     GPIO6C2_MASK << GPIO6C2_SHIFT |
		     GPIO6C1_MASK << GPIO6C1_SHIFT |
		     GPIO6C0_MASK << GPIO6C0_SHIFT,
		     GPIO6C6_SDMMC0_DECTN << GPIO6C6_SHIFT |
		     GPIO6C5_SDMMC0_CMD << GPIO6C5_SHIFT |
		     GPIO6C4_SDMMC0_CLKOUT << GPIO6C4_SHIFT |
		     GPIO6C3_SDMMC0_DATA3 << GPIO6C3_SHIFT |
		     GPIO6C2_SDMMC0_DATA2 << GPIO6C2_SHIFT |
		     GPIO6C1_SDMMC0_DATA1 << GPIO6C1_SHIFT |
		     GPIO6C0_SDMMC0_DATA0 << GPIO6C0_SHIFT);
	rk_clrsetreg(&grf->gpio1_p[SD_BANK][SD_GROUP], SD_PULL_MASK, SD_PULL);
	rk_clrsetreg(&grf->gpio1_e[SD_BANK][SD_GROUP], SD_DRV_MASK,
		     SD_DRV_12MA);

	return 0;
}
