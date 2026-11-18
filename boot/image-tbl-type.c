// SPDX-License-Identifier: GPL-2.0+
/*
 * Table of image-type names for images
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

const table_entry_t uimage_type[] = {
	{	IH_TYPE_AISIMAGE,   "aisimage",   "Davinci AIS image",},
	{	IH_TYPE_FILESYSTEM, "filesystem", "Filesystem Image",	},
	{	IH_TYPE_FIRMWARE,   "firmware",	  "Firmware",		},
	{	IH_TYPE_FLATDT,     "flat_dt",    "Flat Device Tree",	},
	{	IH_TYPE_GPIMAGE,    "gpimage",    "TI Keystone SPL Image",},
	{	IH_TYPE_KERNEL,	    "kernel",	  "Kernel Image",	},
	{	IH_TYPE_KERNEL_NOLOAD, "kernel_noload",  "Kernel Image (no loading done)", },
	{	IH_TYPE_KWBIMAGE,   "kwbimage",   "Kirkwood Boot Image",},
	{	IH_TYPE_IMXIMAGE,   "imximage",   "Freescale i.MX Boot Image",},
	{	IH_TYPE_IMX8IMAGE,  "imx8image",  "NXP i.MX8 Boot Image",},
	{	IH_TYPE_IMX8MIMAGE, "imx8mimage", "NXP i.MX8M Boot Image",},
	{	IH_TYPE_INVALID,    "invalid",	  "Invalid Image",	},
	{	IH_TYPE_MULTI,	    "multi",	  "Multi-File Image",	},
	{	IH_TYPE_OMAPIMAGE,  "omapimage",  "TI OMAP SPL With GP CH",},
	{	IH_TYPE_PBLIMAGE,   "pblimage",   "Freescale PBL Boot Image",},
	{	IH_TYPE_RAMDISK,    "ramdisk",	  "RAMDisk Image",	},
	{	IH_TYPE_SCRIPT,     "script",	  "Script",		},
	{	IH_TYPE_SOCFPGAIMAGE, "socfpgaimage", "Altera SoCFPGA CV/AV preloader",},
	{	IH_TYPE_SOCFPGAIMAGE_V1, "socfpgaimage_v1", "Altera SoCFPGA A10 preloader",},
	{	IH_TYPE_STANDALONE, "standalone", "Standalone Program", },
	{	IH_TYPE_UBLIMAGE,   "ublimage",   "Davinci UBL image",},
	{	IH_TYPE_MXSIMAGE,   "mxsimage",   "Freescale MXS Boot Image",},
	{	IH_TYPE_ATMELIMAGE, "atmelimage", "ATMEL ROM-Boot Image",},
	{	IH_TYPE_X86_SETUP,  "x86_setup",  "x86 setup.bin",    },
	{	IH_TYPE_LPC32XXIMAGE, "lpc32xximage",  "LPC32XX Boot Image", },
	{	IH_TYPE_RKIMAGE,    "rkimage",    "Rockchip Boot Image" },
	{	IH_TYPE_RKSD,       "rksd",       "Rockchip SD Boot Image" },
	{	IH_TYPE_RKSPI,      "rkspi",      "Rockchip SPI Boot Image" },
	{	IH_TYPE_VYBRIDIMAGE, "vybridimage",  "Vybrid Boot Image", },
	{	IH_TYPE_ZYNQIMAGE,  "zynqimage",  "Xilinx Zynq Boot Image" },
	{	IH_TYPE_ZYNQMPIMAGE, "zynqmpimage", "Xilinx ZynqMP Boot Image" },
	{	IH_TYPE_ZYNQMPBIF,  "zynqmpbif",  "Xilinx ZynqMP Boot Image (bif)" },
	{	IH_TYPE_FPGA,       "fpga",       "FPGA Image" },
	{       IH_TYPE_TEE,        "tee",        "Trusted Execution Environment Image",},
	{	IH_TYPE_FIRMWARE_IVT, "firmware_ivt", "Firmware with HABv4 IVT" },
	{       IH_TYPE_PMMC,        "pmmc",        "TI Power Management Micro-Controller Firmware",},
	{	IH_TYPE_STM32IMAGE, "stm32image", "STMicroelectronics STM32 Image" },
	{	IH_TYPE_MTKIMAGE,   "mtk_image",   "MediaTek BootROM loadable Image" },
	{	IH_TYPE_COPRO, "copro", "Coprocessor Image"},
	{	IH_TYPE_SUNXI_EGON, "sunxi_egon",  "Allwinner eGON Boot Image" },
	{	IH_TYPE_SUNXI_TOC0, "sunxi_toc0",  "Allwinner TOC0 Boot Image" },
	{	IH_TYPE_FDT_LEGACY, "fdt_legacy", "legacy Image with Flat Device Tree ", },
	{	IH_TYPE_RENESAS_SPKG, "spkgimage", "Renesas SPKG Image" },
	{	IH_TYPE_STARFIVE_SPL, "sfspl", "StarFive SPL Image" },
	{	IH_TYPE_TFA_BL31, "tfa-bl31",  "TFA BL31 Image", },
	{	IH_TYPE_STM32IMAGE_V2, "stm32imagev2", "STMicroelectronics STM32 Image V2.0" },
	{	-1,		    "",		  "",			},
};
