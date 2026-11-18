/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2013 Google, Inc
 *
 * (C) Copyright 2012
 * Pavel Herrmann <morpheus.ibis@gmail.com>
 * Marek Vasut <marex@denx.de>
 */

#ifndef _DM_PLATDATA_H
#define _DM_PLATDATA_H

#include <linker_lists.h>

/**
 * struct driver_info - Information required to instantiate a device
 *
 * NOTE: Avoid using this except in extreme circumstances, where device tree
 * is not feasible (e.g. serial driver in SPL where <8KB of SRAM is
 * available). U-Boot's driver model uses device tree for configuration.
 *
 * @name:	Driver name (without of-platdata)
 * @drv:	Driver to use (with of-platdata), or NULL if this phase does not
 *	have the node's driver, in which case no device is bound
 * @plat:	Driver-specific platform data. With of-platdata this is the data
 *	which dtoc generates for the driver to read with the dev_prop helpers,
 *	or NULL if there is none
 * @parent_idx:	Index of the parent driver_info structure
 * @seq:	Sequence number of the device, taken by dtoc from its alias in
 *	the devicetree, or -1 if it has none
 */
struct driver_info {
#if IS_ENABLED(CONFIG_OF_PLATDATA)
	const struct driver *drv;
#else
	const char *name;
#endif
	const void *plat;
#if IS_ENABLED(CONFIG_OF_PLATDATA)
	short parent_idx;
	signed char seq;	/* not s8, which is a register name on MIPS */
#endif
};

#if IS_ENABLED(CONFIG_OF_PLATDATA)
#define driver_info_parent_id(driver_info)	driver_info->parent_idx
#define driver_info_seq(_info)			((_info)->seq)
#define driver_info_drv(_info)			((_info)->drv)
#define driver_info_name(_info)	\
	((_info)->drv ? (_info)->drv->name : "(none)")

/* Set the driver for a record, where the driver is always in the build */
#define DM_DRVINFO_REF(_name)	.drv = DM_DRIVER_REF(_name)

/*
 * Set the driver for a record which dtoc generates. The build creates
 * dt-drivers.h with a DM_HAS_DRIVER_<name> for each driver in this phase, so
 * a node whose driver is not built has a NULL driver, rather than a reference
 * which cannot be resolved
 */
#define DM_DRVINFO_DRIVER(_name) \
	.drv = IS_ENABLED(DM_HAS_DRIVER_##_name) ? DM_DRIVER_REF(_name) : NULL
#else
#define driver_info_parent_id(driver_info)	(-1)
#define driver_info_seq(_info)			(-1)
#define driver_info_drv(_info)	lists_driver_lookup_name((_info)->name)
#define driver_info_name(_info)			((_info)->name)
#define DM_DRVINFO_REF(_name)			.name = #_name
#endif

/**
 * struct driver_rt - runtime information set up by U-Boot
 *
 * There is one of these for every driver_info in the linker list, indexed by
 * the driver_info idx value.
 *
 * @dev: Device created from this idx
 */
struct driver_rt {
	struct udevice *dev;
};

/*
 * NOTE: Avoid using these except in extreme circumstances, where device tree
 * is not feasible (e.g. serial driver in SPL where <8KB of SRAM is
 * available). U-Boot's driver model uses device tree for configuration.
 *
 * When of-platdata is in use, U_BOOT_DRVINFO() and U_BOOT_DRVINFOS() cannot be
 * used outside of the dt-plat.c file created by dtoc
 */
#if IS_ENABLED(CONFIG_OF_PLATDATA) && !defined(DT_PLAT_C)
#define U_BOOT_DRVINFO(__name)	_Static_assert(false, \
	"Cannot use U_BOOT_DRVINFO with of-platdata. Please use devicetree instead")
#define U_BOOT_DRVINFOS(__name)	_Static_assert(false, \
	"Cannot use U_BOOT_DRVINFOS with of-platdata. Please use devicetree instead")
#else
#define U_BOOT_DRVINFO(__name)						\
	ll_entry_declare(struct driver_info, __name, driver_info)

/* Declare a list of devices. The argument is a driver_info[] array */
#define U_BOOT_DRVINFOS(__name)						\
	ll_entry_declare_list(struct driver_info, __name, driver_info)
#endif

/*
 * Declare a device for a driver which has no devicetree node, such as a
 * software implementation of an algorithm. This is the whole record: the
 * device uses the driver called @__name, has no platform data and is bound to
 * the root device.
 *
 * This can be used with of-platdata. The record is then named so that it sorts
 * after all those from dtoc, so it does not change the indexes which dtoc
 * works out for its own records. The build checks this
 */
#if IS_ENABLED(CONFIG_OF_PLATDATA)
#define U_BOOT_DRVINFO_NO_NODE(__name)					\
	ll_entry_declare(struct driver_info, zz_##__name, driver_info) = { \
		DM_DRVINFO_REF(__name),					\
		.parent_idx	= -1,					\
		.seq		= -1,					\
	}
#else
#define U_BOOT_DRVINFO_NO_NODE(__name)					\
	ll_entry_declare(struct driver_info, __name, driver_info) = {	\
		DM_DRVINFO_REF(__name),					\
	}
#endif

#endif
