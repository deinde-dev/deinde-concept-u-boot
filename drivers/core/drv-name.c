// SPDX-License-Identifier: GPL-2.0+
/*
 * Looking up and binding a driver by its name
 *
 * Copyright (c) 2013 Google, Inc
 *
 * (C) Copyright 2012
 * Marek Vasut <marex@denx.de>
 */

#define LOG_CATEGORY LOGC_DM

#include <dm/device.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/util.h>
#include <linker_lists.h>
#include <linux/errno.h>
#include <linux/string.h>

struct driver *lists_driver_lookup_name(const char *name)
{
	struct driver *drv =
		ll_entry_start(struct driver, driver);
	const int n_ents = ll_entry_count(struct driver, driver);
	struct driver *entry;

	for (entry = drv; entry != drv + n_ents; entry++) {
		if (!strcmp(name, entry->name))
			return entry;
	}

	/* Not found */
	return NULL;
}

int device_bind_driver(struct udevice *parent, const char *drv_name,
		       const char *dev_name, struct udevice **devp)
{
	return device_bind_driver_to_node(parent, drv_name, dev_name,
					  ofnode_null(), devp);
}

int device_bind_driver_to_node(struct udevice *parent, const char *drv_name,
			       const char *dev_name, ofnode node,
			       struct udevice **devp)
{
	struct driver *drv;
	int ret;

	drv = lists_driver_lookup_name(drv_name);
	if (!drv) {
		dm_warn("Cannot find driver '%s'\n", drv_name);
		return -ENOENT;
	}
	ret = device_bind_with_driver_data(parent, drv, dev_name, 0 /* data */,
					   node, devp);

	return ret;
}
