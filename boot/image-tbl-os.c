// SPDX-License-Identifier: GPL-2.0+
/*
 * Table of operating-system names for images
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

const table_entry_t uimage_os[] = {
	{	IH_OS_INVALID,	"invalid",	"Invalid OS",		},
	{       IH_OS_ARM_TRUSTED_FIRMWARE, "arm-trusted-firmware", "ARM Trusted Firmware"  },
	{	IH_OS_LINUX,	"linux",	"Linux",		},
	{	IH_OS_NETBSD,	"netbsd",	"NetBSD",		},
	{	IH_OS_OSE,	"ose",		"Enea OSE",		},
	{	IH_OS_PLAN9,	"plan9",	"Plan 9",		},
	{	IH_OS_RTEMS,	"rtems",	"RTEMS",		},
	{	IH_OS_TEE,	"tee",		"Trusted Execution Environment" },
	{	IH_OS_U_BOOT,	"u-boot",	"U-Boot",		},
	{	IH_OS_VXWORKS,	"vxworks",	"VxWorks",		},
#if defined(CONFIG_CMD_ELF) || defined(USE_HOSTCC)
	{	IH_OS_QNX,	"qnx",		"QNX",			},
#endif
#if defined(CONFIG_INTEGRITY) || defined(USE_HOSTCC)
	{	IH_OS_INTEGRITY,"integrity",	"INTEGRITY",		},
#endif
#ifdef USE_HOSTCC
	{	IH_OS_4_4BSD,	"4_4bsd",	"4_4BSD",		},
	{	IH_OS_DELL,	"dell",		"Dell",			},
	{	IH_OS_ESIX,	"esix",		"Esix",			},
	{	IH_OS_FREEBSD,	"freebsd",	"FreeBSD",		},
	{	IH_OS_IRIX,	"irix",		"Irix",			},
	{	IH_OS_NCR,	"ncr",		"NCR",			},
	{	IH_OS_OPENBSD,	"openbsd",	"OpenBSD",		},
	{	IH_OS_PSOS,	"psos",		"pSOS",			},
	{	IH_OS_SCO,	"sco",		"SCO",			},
	{	IH_OS_SOLARIS,	"solaris",	"Solaris",		},
	{	IH_OS_SVR4,	"svr4",		"SVR4",			},
#endif
#if defined(CONFIG_BOOTM_OPENRTOS) || defined(USE_HOSTCC)
	{	IH_OS_OPENRTOS,	"openrtos",	"OpenRTOS",		},
#endif
	{	IH_OS_OPENSBI,	"opensbi",	"RISC-V OpenSBI",	},
	{	IH_OS_EFI,	"efi",		"EFI Firmware"		},
#ifdef CONFIG_BOOTM_ELF
	{	IH_OS_ELF,	"elf",		"ELF Image"		},
#endif

	{	-1,		"",		"",			},
};
