/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Reading devicetree properties with or without of-platdata
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#ifndef _DM_PROP_H
#define _DM_PROP_H

/*
 * A driver which supports of-platdata normally needs two copies of the code
 * which reads its configuration: one using the devicetree and one using the
 * C structure which dtoc generates from it. The two tend to drift apart, e.g.
 * with different defaults.
 *
 * These helpers let a driver write that code once. Each one reads the
 * devicetree with dev_read_...() normally. With of-platdata it reads the
 * structure instead, which dtoc generates holding just the properties read
 * with these helpers, plus a flag for each optional one saying whether it is
 * present. Each property is given twice, as its C member name and as its
 * devicetree name, e.g. spi_max_frequency and "spi-max-frequency", since the
 * C preprocessor cannot convert one to the other. dtoc checks that they match.
 *
 * For example::
 *
 *	static int mydrv_of_to_plat(struct udevice *dev)
 *	{
 *		struct mydrv_plat *plat = dev_get_plat(dev);
 *		DTPLAT(mydrv, dt, dev);
 *
 *		plat->base = dev_prop_addr(dev, dt);
 *		plat->freq = dev_prop_u32_default(dev, dt, max_frequency,
 *						  "max-frequency", 50000000);
 *		plat->removable = !dev_prop_bool(dev, dt, non_removable,
 *						 "non-removable");
 *
 *		return dev_prop_clk(dev, dt, clocks, "clocks", 0, &plat->clk);
 *	}
 *
 * where mydrv is the name used in U_BOOT_DRIVER(mydrv). The helpers are
 * named dev_prop_...() to match dev_read_...()
 */

#include <errno.h>
#include <dm/device.h>
#include <dm/read.h>
#include <mapmem.h>

#if IS_ENABLED(CONFIG_OF_PLATDATA)

#include <dt-structs.h>

/**
 * DTPLAT() - Declare a variable holding a device's generated devicetree data
 *
 * Without of-platdata this declares nothing
 *
 * @_drv: Name of the driver, as given to U_BOOT_DRIVER()
 * @_var: Name of the variable to declare
 * @_dev: Device whose data is wanted
 */
#define DTPLAT(_drv, _var, _dev) \
	const struct dtd_drv_##_drv *_var = dev_get_dtplat(_dev)

/**
 * dev_prop_u32_default() - Read a 32-bit integer property, with a default
 *
 * @_dev: Device to read from
 * @_dt: Generated data, as declared by DTPLAT()
 * @_member: C member name of the property, e.g. max_frequency
 * @_name: Devicetree name of the property, e.g. "max-frequency"
 * @_def: Value to use if the property is absent
 * Return: property value, or @_def if the property is absent
 */
#define dev_prop_u32_default(_dev, _dt, _member, _name, _def) \
	((_dt)->has._member ? (_dt)->_member : (_def))

/**
 * dev_prop_bool() - Check whether a boolean property is present
 *
 * @_dev: Device to read from
 * @_dt: Generated data, as declared by DTPLAT()
 * @_member: C member name of the property, e.g. non_removable
 * @_name: Devicetree name of the property, e.g. "non-removable"
 * Return: true if the property is present, false if not
 */
#define dev_prop_bool(_dev, _dt, _member, _name) \
	((_dt)->_member)

/**
 * dev_prop_u32_array() - Read an array of 32-bit integers
 *
 * With of-platdata, the array in the generated data is sized for the node
 * which has the most values, with zeroes after the values of a node which has
 * fewer. So a property shorter than @_count is only detected with a
 * devicetree.
 *
 * @_dev: Device to read from
 * @_dt: Generated data, as declared by DTPLAT()
 * @_member: C member name of the property, e.g. clock_freq_min_max
 * @_name: Devicetree name of the property, e.g. "clock-freq-min-max"
 * @_out: Place to put the values
 * @_count: Number of values to read
 * Return: 0 if OK, -EINVAL if the property is absent, -EOVERFLOW if it has
 * fewer than @_count values
 */
#define dev_prop_u32_array(_dev, _dt, _member, _name, _out, _count) \
	(!(_dt)->has._member ? -EINVAL : \
	 (_count) > ARRAY_SIZE((_dt)->_member) ? -EOVERFLOW : \
	 (memcpy(_out, (_dt)->_member, (_count) * sizeof(u32)), 0))

/**
 * dev_prop_addr() - Read the address from a device's 'reg' property
 *
 * With of-platdata, the address is used as it is in the devicetree, without
 * translation through the 'ranges' properties of parent nodes. dtoc checks
 * that each node whose driver reads its address has a 'reg' property.
 *
 * @_dev: Device to read from
 * @_dt: Generated data, as declared by DTPLAT()
 * Return: address, or FDT_ADDR_T_NONE if there is no 'reg' property
 */
#define dev_prop_addr(_dev, _dt) \
	((fdt_addr_t)(_dt)->reg[0])

/**
 * dev_prop_addr_ptr() - Read the address from 'reg' as a pointer
 *
 * This is the same as dev_prop_addr() but returns a pointer, as with
 * dev_read_addr_ptr()
 *
 * @_dev: Device to read from
 * @_dt: Generated data, as declared by DTPLAT()
 * Return: pointer to the address, or NULL if there is no 'reg' property
 */
#define dev_prop_addr_ptr(_dev, _dt) \
	map_sysmem((_dt)->reg[0], 0)

/**
 * dev_prop_clk() - Get a clock from a device's 'clocks' property
 *
 * @_dev: Device to read from
 * @_dt: Generated data, as declared by DTPLAT()
 * @_member: C member name of the property, i.e. clocks
 * @_name: Devicetree name of the property, i.e. "clocks"
 * @_idx: Index of the clock to get, within the property
 * @_clkp: Returns the clock
 * Return: 0 if OK, -ENOENT if there is no such clock, or another error from
 * the clock uclass
 */
#define dev_prop_clk(_dev, _dt, _member, _name, _idx, _clkp) \
	(!(_dt)->has._member || (_idx) >= ARRAY_SIZE((_dt)->_member) ? \
	 -ENOENT : clk_get_by_phandle(_dev, &(_dt)->_member[_idx], _clkp))

#else /* !OF_PLATDATA */

#define DTPLAT(_drv, _var, _dev)

#define dev_prop_u32_default(_dev, _dt, _member, _name, _def) \
	dev_read_u32_default(_dev, _name, _def)

#define dev_prop_bool(_dev, _dt, _member, _name) \
	dev_read_bool(_dev, _name)

#define dev_prop_u32_array(_dev, _dt, _member, _name, _out, _count) \
	dev_read_u32_array(_dev, _name, _out, _count)

#define dev_prop_addr(_dev, _dt) \
	dev_read_addr(_dev)

#define dev_prop_addr_ptr(_dev, _dt) \
	dev_read_addr_ptr(_dev)

#define dev_prop_clk(_dev, _dt, _member, _name, _idx, _clkp) \
	clk_get_by_index(_dev, _idx, _clkp)

#endif /* OF_PLATDATA */

#endif
