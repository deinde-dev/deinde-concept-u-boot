// SPDX-License-Identifier: GPL-2.0+
/*
 * MON, the resident phase which runs at EL3 as the secure monitor
 *
 * SPL starts MON as it would TF-A's BL31. MON sets up the parts of the
 * machine which only EL3 can, then starts U-Boot proper at EL2 and stays in
 * memory to handle SMCs from it and from the OS.
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <atf_common.h>
#include <asm/io.h>
#include <asm/psci.h>
#include <asm/secure.h>
#include <asm/system.h>
#include <linux/stringify.h>

#define sysreg_write(reg, val) \
	asm volatile("msr " __stringify(reg) ", %0" : : "r" ((ulong)(val)))

/**
 * find_bl33() - Find the entry point of U-Boot proper in SPL's parameters
 *
 * SPL passes either the older bl31_params or the newer bl_params list
 *
 * @params: Parameters from SPL
 * Return: entry-point information for BL33, or NULL if there is none
 */
static struct entry_point_info *find_bl33(void *params)
{
	struct param_header *hdr = params;

	if (!hdr)
		return NULL;
	if (hdr->type == ATF_PARAM_BL31) {
		struct bl31_params *v1 = params;

		return v1->bl33_ep_info;
	}
	if (hdr->type == ATF_PARAM_BL_PARAMS) {
		struct bl_params *v2 = params;
		struct bl_params_node *node;

		for_each_bl_params_node(v2, node) {
			if (node->image_id == ATF_BL33_IMAGE_ID)
				return node->ep_info;
		}
	}

	return NULL;
}

/* Report PSCI v0.2, which needs no PSCI_FEATURES call */
u32 __secure psci_version(void)
{
	return ARM_PSCI_VER_0_2;
}

/**
 * mon_board_init() - Set up the board before U-Boot proper starts
 *
 * Boards can provide this to set up parts of the SoC which only the secure
 * monitor can
 */
__weak void mon_board_init(void)
{
}

/**
 * mon_main() - Set up the machine and start U-Boot proper
 *
 * @params: Parameters from SPL, giving the entry point of U-Boot proper
 * @blob: Devicetree for U-Boot proper, which SPL passes on
 */
void __noreturn mon_main(void *params, const void *blob)
{
	struct entry_point_info *bl33 = find_bl33(params);

	/* Only EL3 can tell the lower levels how fast the timer runs */
	if (CONFIG_COUNTER_FREQUENCY)
		sysreg_write(cntfrq_el0, CONFIG_COUNTER_FREQUENCY);
	mon_board_init();

	/* Nothing to start, so wait for an SMC which will never come */
	while (!bl33)
		;

	/* This starts EL2 with exceptions masked, which is what SPL asks for */
	armv8_switch_to_el2(bl33->args.arg0, 0, 0, 0, bl33->pc, ES_TO_AARCH64);
}
