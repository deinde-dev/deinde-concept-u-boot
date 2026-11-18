/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Values from the /chosen node, for use with of-platdata
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#ifndef __DT_CHOSEN_H
#define __DT_CHOSEN_H

/**
 * struct dt_boot_order - an entry in the SPL boot order
 *
 * With of-platdata, dtoc provides the entries of the u-boot,spl-boot-order
 * property in DT_CHOSEN_SPL_BOOT_ORDER, as initialisers for this struct. The
 * macro is empty if there is no such property.
 *
 * @path: Devicetree path of the node, or NULL for 'same-as-spl', which means
 *	the device which SPL was loaded from
 * @idx: Index of the driver_info record for the node, or -1 if it has none
 */
struct dt_boot_order {
	const char *path;
	int idx;
};

#if IS_ENABLED(CONFIG_OF_PLATDATA)
#include <generated/dt-chosen.h>
#else
/* With a devicetree, the boot order is read from the /chosen node */
#define DT_CHOSEN_SPL_BOOT_ORDER
#endif

#endif
