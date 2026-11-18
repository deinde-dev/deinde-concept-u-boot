// SPDX-License-Identifier: GPL-2.0+
/*
 * Sandbox pin controller for testing pin settings, with a devicetree and with
 * of-platdata
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <asm/test.h>
#include <dm/pinctrl.h>

/* The pin-config properties which this driver understands */
static const struct pinconf_param sandbox_pins_params[] = {
	{ "bias-disable", PIN_CONFIG_BIAS_DISABLE, 0 },
	{ "bias-pull-up", PIN_CONFIG_BIAS_PULL_UP, 1 },
	{ "drive-strength", PIN_CONFIG_DRIVE_STRENGTH, 0 },
};

/* Start a new pin, returning NULL if there is no room for it */
static struct sandbox_pin *sandbox_pins_add(struct udevice *dev, uint pin,
					    uint func)
{
	struct sandbox_pins_priv *priv = dev_get_priv(dev);
	struct sandbox_pin *rec;

	if (priv->count == SANDBOX_PINS_MAX)
		return NULL;
	rec = &priv->pin[priv->count++];
	rec->pin = pin;
	rec->func = func;
	rec->num_params = 0;

	return rec;
}

/* Record a config parameter for a pin */
static int sandbox_pins_add_param(struct sandbox_pin *rec, uint param,
				  uint arg)
{
	if (rec->num_params == SANDBOX_PIN_PARAMS)
		return -ENOSPC;
	rec->param[rec->num_params] = param;
	rec->arg[rec->num_params++] = arg;

	return 0;
}

/* Read the config for a pin from a node holding pin-config properties */
static int sandbox_pins_read_conf(struct sandbox_pin *rec, ofnode node)
{
	const struct pinconf_param *param;
	int ret, i;

	for (i = 0; i < ARRAY_SIZE(sandbox_pins_params); i++) {
		u32 arg;

		param = &sandbox_pins_params[i];
		if (!ofnode_has_property(node, param->property))
			continue;
		if (ofnode_read_u32(node, param->property, &arg))
			arg = param->default_value;
		ret = sandbox_pins_add_param(rec, param->param, arg);
		if (ret)
			return ret;
	}

	return 0;
}

/* With a devicetree, read the pins from a node which a device refers to */
static int sandbox_pins_set_state(struct udevice *dev, struct udevice *config)
{
	u32 cells[SANDBOX_PINS_MAX * 3];
	int ret, count, i;

	if (!IS_ENABLED(CONFIG_OF_REAL))
		return -ENOSYS;

	count = dev_read_size(config, "sandbox,pins");
	if (count < 0)
		return count;
	count /= sizeof(u32);
	if (count > ARRAY_SIZE(cells))
		return -ENOSPC;
	ret = dev_read_u32_array(config, "sandbox,pins", cells, count);
	if (ret)
		return ret;

	for (i = 0; i < count; i += 3) {
		struct sandbox_pin *rec;

		rec = sandbox_pins_add(dev, cells[i], cells[i + 1]);
		if (!rec)
			return -ENOSPC;
		ret = sandbox_pins_read_conf(rec,
					     ofnode_get_by_phandle(cells[i + 2]));
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * With of-platdata, dtoc provides the pins in the form given by DM_PINS()
 * below: the pin and function, as in the sandbox,pins property, then the
 * position of the pin's config
 */
static int sandbox_pins_set_pins(struct udevice *dev,
				 const struct dm_pin_state *state)
{
	const u16 *cell = state->pins;
	int ret, i, j;

	for (i = 0; i < state->count; i++, cell += 3) {
		const u16 *conf = state->conf + cell[2];
		struct sandbox_pin *rec;

		rec = sandbox_pins_add(dev, cell[0], cell[1]);
		if (!rec)
			return -ENOSPC;

		/* The config is a count, then a parameter and arg for each */
		for (j = 0; j < conf[0]; j++) {
			ret = sandbox_pins_add_param(rec, conf[1 + 2 * j],
						     conf[2 + 2 * j]);
			if (ret)
				return ret;
		}
	}

	return 0;
}

/* This has no named pins, but the 'pinmux' command needs the method */
static int sandbox_pins_get_pins_count(struct udevice *dev)
{
	return 0;
}

static const struct pinctrl_ops sandbox_pins_ops = {
	.get_pins_count	= sandbox_pins_get_pins_count,
	.set_state	= sandbox_pins_set_state,
	.set_pins	= sandbox_pins_set_pins,
};

static __maybe_unused const struct udevice_id sandbox_pins_ids[] = {
	{ .compatible = "sandbox,pins-test" },
	{ }
};

U_BOOT_DRIVER(sandbox_pins_test) = {
	.name		= "sandbox_pins_test",
	.id		= UCLASS_PINCTRL,
	.of_match	= of_match_ptr(sandbox_pins_ids),
	.ops		= &sandbox_pins_ops,
	.priv_auto	= sizeof(struct sandbox_pins_priv),
	DM_PINS("sandbox,pins", 3, 2)
};
