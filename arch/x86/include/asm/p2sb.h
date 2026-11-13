/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2020 Google LLC
 */

#ifndef ASM_P2SB_H
#define ASM_P2SB_H

/* Platform data for the P2SB */
struct p2sb_plat {
	ulong mmio_base;
	pci_dev_t bdf;
};

#endif	/* ASM_P2SB_H */
