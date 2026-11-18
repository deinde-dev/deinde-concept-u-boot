// SPDX-License-Identifier: GPL-2.0+
/*
 * Table of compression names for images
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

const table_entry_t uimage_comp[] = {
	{	IH_COMP_NONE,	"none",		"uncompressed",		},
	{	IH_COMP_BZIP2,	"bzip2",	"bzip2 compressed",	},
	{	IH_COMP_GZIP,	"gzip",		"gzip compressed",	},
	{	IH_COMP_LZMA,	"lzma",		"lzma compressed",	},
	{	IH_COMP_LZO,	"lzo",		"lzo compressed",	},
	{	IH_COMP_LZ4,	"lz4",		"lz4 compressed",	},
	{	IH_COMP_ZSTD,	"zstd",		"zstd compressed",	},
	{	-1,		"",		"",			},
};
