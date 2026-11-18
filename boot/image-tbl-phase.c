// SPDX-License-Identifier: GPL-2.0+
/*
 * Table of phase names for images
 *
 * Each table is in its own file so that the linker can drop the names in the
 * tables which are not used. This is useful in xPL, which often needs just one
 * of them.
 *
 * (C) Copyright 2008 Semihalf
 *
 * (C) Copyright 2000-2006
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <image.h>

const table_entry_t uimage_phase[] = {
	{	IH_PHASE_NONE,	"none",		"any",		},
	{	IH_PHASE_U_BOOT, "u-boot",	"U-Boot phase",	},
	{	IH_PHASE_SPL,	"spl",		"SPL Phase",	},
	{	-1,		"",		"",		},
};
