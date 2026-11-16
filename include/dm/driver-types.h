/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Types and macros shared by struct driver and struct uclass_driver
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#ifndef _DM_DRIVER_TYPES_H
#define _DM_DRIVER_TYPES_H

/*
 * dm_remove_ptr() - Use a method only if devices can be removed
 *
 * Without DM_DEVICE_REMOVE nothing calls the methods which remove or unbind a
 * device, or destroy a uclass. Use this to set those methods in struct driver
 * and struct uclass_driver, e.g.:
 *
 *	.remove	= dm_remove_ptr(mmc_remove),
 */
#define dm_remove_ptr(_ptr)	(_ptr)

#endif
