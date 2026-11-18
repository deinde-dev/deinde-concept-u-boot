// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2013 Google, Inc
 *
 * (C) Copyright 2012
 * Marek Vasut <marex@denx.de>
 */

#define LOG_CATEGORY LOGC_DM

#include <bootstage.h>
#include <debug_uart.h>
#include <errno.h>
#include <log.h>
#include <dm/device.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/platdata.h>
#include <dm/uclass.h>
#include <dm/util.h>
#include <fdtdec.h>
#include <linux/compiler.h>

struct uclass_driver *lists_uclass_lookup(enum uclass_id id)
{
	struct uclass_driver *uclass =
		ll_entry_start(struct uclass_driver, uclass_driver);
	const int n_ents = ll_entry_count(struct uclass_driver, uclass_driver);
	struct uclass_driver *entry;

	for (entry = uclass; entry != uclass + n_ents; entry++) {
		if (entry->id == id)
			return entry;
	}

	return NULL;
}

int lists_bind_drivers(struct udevice *parent, bool pre_reloc_only)
{
	struct driver_info *info =
		ll_entry_start(struct driver_info, driver_info);
	const int n_ents = ll_entry_count(struct driver_info, driver_info);
	int result = 0;
	int idx;

	/*
	 * Bind a device for each driver_info record. If binding fails, carry
	 * on with the others, but return the error.
	 *
	 * With of-platdata, dtoc names each record so that it comes after that
	 * of its parent, so a parent is always bound before its children
	 */
	for (idx = 0; idx < n_ents; idx++) {
		struct udevice *par = parent;
		const struct driver_info *entry = info + idx;
		struct driver_rt *drt = gd_dm_driver_rt() + idx;
		struct udevice *dev;
		int ret;

		if (IS_ENABLED(CONFIG_OF_PLATDATA)) {
			int parent_idx = driver_info_parent_id(entry);

			if (drt->dev)
				continue;

			if (IS_ENABLED(CONFIG_OF_PLATDATA_PARENT) &&
			    parent_idx != -1) {
				struct driver_rt *rt = gd_dm_driver_rt();

				par = rt[parent_idx].dev;

				/* Without its parent, the device cannot work */
				if (!par) {
					if (!result)
						result = -ENOENT;
					continue;
				}
			}
		}
		ret = device_bind_by_name(par, pre_reloc_only, entry, &dev);
		if (!ret) {
			if (IS_ENABLED(CONFIG_OF_PLATDATA))
				drt->dev = dev;
		} else if (ret != -EPERM) {
			dm_warn("No match for driver '%s'\n",
				driver_info_name(entry));
			if (!result || ret != -ENOENT)
				result = ret;
		}
	}

	return result;
}

int lists_highest_alias_seq(enum uclass_id id)
{
	const struct driver_info *info =
		ll_entry_start(struct driver_info, driver_info);
	const int n_ents = ll_entry_count(struct driver_info, driver_info);
	int max = -1;
	int idx;

	for (idx = 0; idx < n_ents; idx++, info++) {
		const struct driver *drv = driver_info_drv(info);

		if (drv && drv->id == id && driver_info_seq(info) > max)
			max = driver_info_seq(info);
	}

	return max;
}

#if IS_ENABLED(CONFIG_OF_REAL)
/**
 * driver_check_compatible() - Check if a driver matches a compatible string
 *
 * @param of_match:	List of compatible strings to match
 * @param of_idp:	Returns the match that was found
 * @param compat:	The compatible string to search for
 * Return: 0 if there is a match, -ENOENT if no match
 */
static int driver_check_compatible(const struct udevice_id *of_match,
				   const struct udevice_id **of_idp,
				   const char *compat)
{
	if (!of_match)
		return -ENOENT;

	while (of_match->compatible) {
		if (!strcmp(of_match->compatible, compat)) {
			*of_idp = of_match;
			return 0;
		}
		of_match++;
	}

	return -ENOENT;
}

/**
 * enum dm_bind_stat - Things counted while binding devices
 *
 * @DM_BIND_NODES: Nodes considered for binding
 * @DM_BIND_NOCOMPAT: Nodes which have no compatible string
 * @DM_BIND_PRERELOC: Nodes which ask to be bound before relocation
 * @DM_BIND_COMPATS: Compatible strings looked at
 * @DM_BIND_CHECKS: Comparisons of a compatible string against a driver
 * @DM_BIND_COUNT: Number of counts
 */
enum dm_bind_stat {
	DM_BIND_NODES,
	DM_BIND_NOCOMPAT,
	DM_BIND_PRERELOC,
	DM_BIND_COMPATS,
	DM_BIND_CHECKS,

	DM_BIND_COUNT,
};

#ifdef CONFIG_BOOTSTAGE_ACCUM_DM
/**
 * struct dm_bind_stats - Counts of the work done in binding devices
 *
 * These are for diagnosis only, to show where the time goes on a board which
 * is slow to bind. Nothing in normal operation reads them, which is why they
 * are behind BOOTSTAGE_ACCUM_DM
 *
 * @count: Number of times each thing happened, indexed by enum dm_bind_stat
 */
struct dm_bind_stats {
	ulong count[DM_BIND_COUNT];
};

/* The counts live in .data since they are updated before relocation */
static struct dm_bind_stats dm_bind __section(".data");

static void dm_bind_inc(enum dm_bind_stat stat)
{
	dm_bind.count[stat]++;
}

void lists_bind_report(void)
{
	if (!dm_bind.count[DM_BIND_NODES])
		return;
	printf("\nBinding: %lu nodes (%lu without a compatible), %lu pre-reloc\n",
	       dm_bind.count[DM_BIND_NODES], dm_bind.count[DM_BIND_NOCOMPAT],
	       dm_bind.count[DM_BIND_PRERELOC]);
	printf("         %lu compatible strings, %lu driver comparisons\n",
	       dm_bind.count[DM_BIND_COMPATS], dm_bind.count[DM_BIND_CHECKS]);
}
#else
static inline void dm_bind_inc(enum dm_bind_stat stat) {}
#endif /* BOOTSTAGE_ACCUM_DM */

int lists_bind_fdt(struct udevice *parent, ofnode node, struct udevice **devp,
		   struct driver *drv, bool pre_reloc_only)
{
	struct driver *driver = ll_entry_start(struct driver, driver);
	const int n_ents = ll_entry_count(struct driver, driver);
	const struct udevice_id *id;
	struct driver *entry;
	struct udevice *dev;
	const char *name, *compat_list, *compat;
	bool node_pre_reloc = false;
	int compat_length, i;
	int ret = 0;

	if (IS_ENABLED(CONFIG_BOOTSTAGE_ACCUM_DM)) {
		bootstage_start(BOOTSTAGE_ID_ACCUM_DM_BIND, "dm_bind");
		dm_bind_inc(DM_BIND_NODES);
	}

	/*
	 * Work out once whether the node itself asks to be bound before
	 * relocation, rather than for each driver which matches it
	 */
	if (pre_reloc_only) {
		node_pre_reloc = ofnode_pre_reloc(node);
		if (node_pre_reloc)
			dm_bind_inc(DM_BIND_PRERELOC);
	}

	if (devp)
		*devp = NULL;
	name = ofnode_get_name(node);
	log_debug("bind node %s\n", name);

	compat_list = ofnode_get_property(node, "compatible", &compat_length);
	if (!compat_list) {
		if (IS_ENABLED(CONFIG_BOOTSTAGE_ACCUM_DM)) {
			dm_bind_inc(DM_BIND_NOCOMPAT);
			bootstage_accum(BOOTSTAGE_ID_ACCUM_DM_BIND);
		}
		if (compat_length == -FDT_ERR_NOTFOUND) {
			log_debug("Device '%s' has no compatible string\n",
				  name);
			return 0;
		}

		dm_warn("Device tree error at node '%s'\n", name);
		return compat_length;
	}

	/*
	 * Walk through the compatible string list, attempting to match each
	 * compatible string in order such that we match in order of priority
	 * from the first string to the last.
	 */
	for (i = 0; i < compat_length; i += strlen(compat) + 1) {
		compat = compat_list + i;
		log_debug("   - attempt to match compatible string '%s'\n",
			  compat);

		dm_bind_inc(DM_BIND_COMPATS);

		for (entry = driver; entry != driver + n_ents; entry++) {
			/* Search for drivers with matching drv or existing of_match */
			if (drv) {
				if (drv != entry)
					continue;
			} else if (!entry->of_match) {
				continue;
			}

			/*
			 * Before relocation a device can only bind if its node
			 * asks for it, or the driver does. Check the flag first,
			 * since comparing the compatible strings is far more
			 * expensive: it runs over every driver for every node,
			 * with the caches still off
			 */
			if (pre_reloc_only && !node_pre_reloc &&
			    !(entry->flags & DM_FLAG_PRE_RELOC)) {
				log_debug("Skipping device pre-relocation\n");
				continue;
			}

			id = NULL;
			if (entry->of_match) {
				dm_bind_inc(DM_BIND_CHECKS);
				ret = driver_check_compatible(entry->of_match, &id,
							      compat);
				if (ret)
					continue;
				log_debug("   - found match at driver '%s' for '%s'\n",
					  entry->name, id->compatible);
			}

			if (IS_ENABLED(CONFIG_BOOTSTAGE_ACCUM_DM))
				bootstage_start(BOOTSTAGE_ID_ACCUM_DM_DEVBIND,
						"dm_devbind");
			ret = device_bind_with_driver_data(parent, entry, name,
							   id ? id->data : 0, node,
							   &dev);
			if (IS_ENABLED(CONFIG_BOOTSTAGE_ACCUM_DM))
				bootstage_accum(BOOTSTAGE_ID_ACCUM_DM_DEVBIND);
			if (!drv && ret == -ENODEV) {
				log_debug("   - Driver '%s' refuses to bind\n", entry->name);
				continue;
			}
			if (ret) {
				dm_warn("Error binding driver '%s': %d\n", entry->name,
					ret);
				if (IS_ENABLED(CONFIG_BOOTSTAGE_ACCUM_DM))
					bootstage_accum(BOOTSTAGE_ID_ACCUM_DM_BIND);
				return log_msg_ret("bind", ret);
			}

			if (devp)
				*devp = dev;

			if (IS_ENABLED(CONFIG_BOOTSTAGE_ACCUM_DM))
				bootstage_accum(BOOTSTAGE_ID_ACCUM_DM_BIND);

			return 0;
		}
	}

	if (ret != -ENODEV)
		log_debug("No match for node '%s'\n", name);

	if (IS_ENABLED(CONFIG_BOOTSTAGE_ACCUM_DM))
		bootstage_accum(BOOTSTAGE_ID_ACCUM_DM_BIND);

	return 0;
}
#endif
