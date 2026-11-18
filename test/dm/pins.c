// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for the pin settings of a device, which are the same with or without
 * of-platdata
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <asm/test.h>
#include <dm/pinctrl.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>

/* Test that the pins of a device are set up when it is probed */
static int dm_test_pins(struct unit_test_state *uts)
{
	struct sandbox_pins_priv *priv;
	struct udevice *pctl, *dev;
	struct sandbox_pin *pin;
	struct driver *drv;

	ut_assertok(uclass_get_device_by_driver(UCLASS_PINCTRL,
						DM_DRIVER_GET(sandbox_pins_test),
						&pctl));
	priv = dev_get_priv(pctl);
	ut_asserteq(0, priv->count);

	/* The device has two nodes of pins, with three pins between them */
	drv = DM_DRIVER_GET(sandbox_dev_prop_single);
	ut_assertok(uclass_get_device_by_driver(UCLASS_MISC, drv, &dev));
	ut_asserteq(3, priv->count);

	pin = &priv->pin[0];
	ut_asserteq(1, pin->pin);
	ut_asserteq(2, pin->func);
	ut_asserteq(1, pin->num_params);
	ut_asserteq(PIN_CONFIG_BIAS_PULL_UP, pin->param[0]);
	ut_asserteq(1, pin->arg[0]);

	pin = &priv->pin[1];
	ut_asserteq(3, pin->pin);
	ut_asserteq(4, pin->func);
	ut_asserteq(2, pin->num_params);
	ut_asserteq(PIN_CONFIG_BIAS_DISABLE, pin->param[0]);
	ut_asserteq(0, pin->arg[0]);
	ut_asserteq(PIN_CONFIG_DRIVE_STRENGTH, pin->param[1]);
	ut_asserteq(12, pin->arg[1]);

	pin = &priv->pin[2];
	ut_asserteq(5, pin->pin);
	ut_asserteq(6, pin->func);
	ut_asserteq(1, pin->num_params);
	ut_asserteq(PIN_CONFIG_BIAS_PULL_UP, pin->param[0]);

	return 0;
}
DM_TEST(dm_test_pins, IS_ENABLED(CONFIG_OF_PLATDATA) ? UTF_SCAN_PDATA :
	UTF_SCAN_FDT);
