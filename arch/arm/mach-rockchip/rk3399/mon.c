// SPDX-License-Identifier: GPL-2.0+
/*
 * RK3399 support for MON, the secure monitor
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 *
 * mon_board_init() and the registers it uses are based on secure_sgrf_init()
 * in plat/rockchip/rk3399/drivers/secure/ of TF-A, which is under the
 * BSD-3-Clause licence:
 *
 * Copyright (c) 2016-2019, ARM Limited and Contributors. All rights reserved.
 */

#include <asm/io.h>

#define RK3399_SGRF_BASE	0xff330000

/* Secure GRF registers which say which masters and slaves are secure */
#define SGRF_SOC_CON(n)		(0xe00c + ((n) - 3) * 4)
#define SGRF_PMU_SLV_CON(n)	(0xc240 + (n) * 4)
#define SGRF_SLV_SECURE_CON(n)	(0xe3c0 + (n) * 4)

/* The top 16 bits of each register are a write mask for the bottom 16 */
#define SGRF_WMSK		0xffff0000

/*
 * mon_board_init() - Let the non-secure side reach the whole SoC
 *
 * Out of reset, many bus masters and slaves are secure-only, so U-Boot proper
 * and the OS, which run in the non-secure state, cannot use them
 */
void mon_board_init(void)
{
	void __iomem *sgrf = (void __iomem *)RK3399_SGRF_BASE;
	int i;

	for (i = 5; i <= 7; i++)
		writel(SGRF_WMSK | 0xffff, sgrf + SGRF_SOC_CON(i));
	for (i = 0; i <= 1; i++)
		writel(SGRF_WMSK, sgrf + SGRF_PMU_SLV_CON(i));
	for (i = 0; i <= 4; i++)
		writel(SGRF_WMSK, sgrf + SGRF_SLV_SECURE_CON(i));
}
