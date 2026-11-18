// SPDX-License-Identifier: GPL-2.0+
/*
 * Table of architecture names for images
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

const table_entry_t uimage_arch[] = {
	{	IH_ARCH_INVALID,	"invalid",	"Invalid ARCH",	},
	{	IH_ARCH_ALPHA,		"alpha",	"Alpha",	},
	{	IH_ARCH_ARM,		"arm",		"ARM",		},
	{	IH_ARCH_I386,		"x86",		"Intel x86",	},
	{	IH_ARCH_IA64,		"ia64",		"IA64",		},
	{	IH_ARCH_M68K,		"m68k",		"M68K",		},
	{	IH_ARCH_MICROBLAZE,	"microblaze",	"MicroBlaze",	},
	{	IH_ARCH_MIPS,		"mips",		"MIPS",		},
	{	IH_ARCH_MIPS64,		"mips64",	"MIPS 64 Bit",	},
	{	IH_ARCH_NIOS2,		"nios2",	"NIOS II",	},
	{	IH_ARCH_PPC,		"powerpc",	"PowerPC",	},
	{	IH_ARCH_PPC,		"ppc",		"PowerPC",	},
	{	IH_ARCH_S390,		"s390",		"IBM S390",	},
	{	IH_ARCH_SH,		"sh",		"SuperH",	},
	{	IH_ARCH_SPARC,		"sparc",	"SPARC",	},
	{	IH_ARCH_SPARC64,	"sparc64",	"SPARC 64 Bit",	},
	{	IH_ARCH_BLACKFIN,	"blackfin",	"Blackfin",	},
	{	IH_ARCH_AVR32,		"avr32",	"AVR32",	},
	{	IH_ARCH_NDS32,		"nds32",	"NDS32",	},
	{	IH_ARCH_OPENRISC,	"or1k",		"OpenRISC 1000",},
	{	IH_ARCH_SANDBOX,	"sandbox",	"Sandbox",	},
	{	IH_ARCH_ARM64,		"arm64",	"AArch64",	},
	{	IH_ARCH_ARC,		"arc",		"ARC",		},
	{	IH_ARCH_X86_64,		"x86_64",	"AMD x86_64",	},
	{	IH_ARCH_XTENSA,		"xtensa",	"Xtensa",	},
	{	IH_ARCH_RISCV,		"riscv",	"RISC-V",	},
	{	-1,			"",		"",		},
};
