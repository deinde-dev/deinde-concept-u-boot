/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Bluewater Systems Snapper 9260 and 9G20 modules
 *
 * (C) Copyright 2011 Bluewater Systems
 *   Author: Andre Renaud <andre@bluewatersys.com>
 *   Author: Ryan Mallon <ryan@bluewatersys.com>
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <asm/hardware.h>

/* ARM asynchronous clock */
#define CFG_SYS_AT91_MAIN_CLOCK		18432000 /* External Crystal, in Hz */
#define CFG_SYS_AT91_SLOW_CLOCK		32768

/* SDRAM */
#define CFG_SYS_SDRAM_BASE		ATMEL_BASE_CS1

#endif /* __CONFIG_H */
