/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Types and macros shared by struct driver and struct uclass_driver
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#ifndef _DM_DRIVER_TYPES_H
#define _DM_DRIVER_TYPES_H

#include <linux/kconfig.h>

/**
 * struct dm_absent - A member which is left out of a struct in some builds
 *
 * This takes up no space. A member of this type is set using the macro for
 * that member, e.g. dm_remove_ptr(), so that the driver needs no #if
 */
struct dm_absent {
};

/*
 * DM_ABSENT() - Value for a member which is left out
 *
 * This refers to @_val without using it, so that the compiler does not warn
 * about an unused function or table, and leaves it out of the image
 */
#define DM_ABSENT(_val)	\
	__builtin_choose_expr(1, (struct dm_absent){ }, (_val))

/*
 * With of-platdata and no devicetree, each device is declared by dtoc, so
 * nothing reads the driver's match table and it only wastes space
 */
#define DM_OF_MATCH_UNUSED	\
	(IS_ENABLED(CONFIG_OF_PLATDATA) && !IS_ENABLED(CONFIG_OF_REAL))

/*
 * dm_remove_ptr() - Use a method only if devices can be removed
 *
 * Without DM_DEVICE_REMOVE nothing calls the methods which remove or unbind a
 * device, or destroy a uclass, so struct driver and struct uclass_driver leave
 * them out. Use this to set those methods, e.g.:
 *
 *	.remove	= dm_remove_ptr(mmc_remove),
 */
#if IS_ENABLED(CONFIG_DM_DEVICE_REMOVE)
#define dm_remove_ptr(_ptr)	(_ptr)
#else
#define dm_remove_ptr(_ptr)	DM_ABSENT(_ptr)
#endif

#endif
