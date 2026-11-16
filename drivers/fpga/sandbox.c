// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2022 Alexander Dahl <post@lespocky.de>
 */

#include <dm.h>

static __maybe_unused const struct udevice_id sandbox_fpga_match[] = {
	{ .compatible = "sandbox,fpga" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(sandbox_fpga) = {
	.name	= "sandbox_fpga",
	.id	= UCLASS_FPGA,
	.of_match = of_match_ptr(sandbox_fpga_match),
};
