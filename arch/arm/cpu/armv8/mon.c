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
#include <fdt_support.h>
#include <asm/gic.h>
#include <asm/gic-v3.h>
#include <asm/io.h>
#include <asm/psci.h>
#include <asm/secure.h>
#include <asm/system.h>
#include <linux/libfdt.h>
#include <linux/stringify.h>

#define sysreg_write(reg, val) \
	asm volatile("msr " __stringify(reg) ", %0" : : "r" ((ulong)(val)))
#define sysreg_read(reg) ({ \
	ulong __val; \
	asm volatile("mrs %0, " __stringify(reg) : "=r" (__val)); \
	__val; \
})

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

/* Values used to set up the GICv3 from the secure state */
enum {
	/* MPIDR_EL1: Aff2-Aff0 are in bits 23:0 and Aff3 in bits 39:32 */
	MPIDR_AFF_LOW_MASK	= 0xffffff,
	MPIDR_AFF3_SHIFT	= 32,
	MPIDR_AFF3_MASK		= 0xff,

	/* GICD_CTLR, when the GIC has two security states */
	GICD_CTLR_ENABLE_G0	= BIT(0),
	GICD_CTLR_ENABLE_G1NS	= BIT(1),
	GICD_CTLR_ENABLE_G1S	= BIT(2),
	GICD_CTLR_ARE_S		= BIT(4),
	GICD_CTLR_ARE_NS	= BIT(5),

	/* GICD_TYPER: number of registers of 32 interrupts, less one */
	GICD_TYPER_ITLINES_MASK	= 0x1f,

	/* GICR_TYPER holds the affinity of its CPU in the top 32 bits */
	GICR_TYPER_AFF_SHIFT	= 32,

	/* Each redistributor has two 64KB frames: RD_base then SGI_base */
	GICR_FRAME_SIZE		= 0x10000,
	GICR_SIZE		= 2 * GICR_FRAME_SIZE,

	/* ICC_SRE_ELx */
	ICC_SRE_SRE		= BIT(0),	/* use the system registers */
	ICC_SRE_DFB		= BIT(1),	/* disable FIQ bypass */
	ICC_SRE_DIB		= BIT(2),	/* disable IRQ bypass */
	ICC_SRE_ENABLE		= BIT(3),	/* lower ELs may use them too */
	ICC_SRE_ALL		= ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB |
				  ICC_SRE_ENABLE,

	/* ICC_IGRPEN1_EL3 */
	ICC_IGRPEN1_G1NS	= BIT(0),
	ICC_IGRPEN1_G1S		= BIT(1),
};

/**
 * gicv3_find_fdt() - Find the GICv3 in the devicetree
 *
 * The first region of the node is the distributor and the second is the
 * redistributors. No address translation is done, so the node must be at the
 * top level, or below buses which map their addresses directly.
 *
 * @blob: Devicetree to look in
 * @gicdp: Returns the address of the distributor
 * @gicrp: Returns the address of the redistributors
 * Return: 0 if OK, -ve FDT_ERR_... value if there is no usable node
 */
static int gicv3_find_fdt(const void *blob, ulong *gicdp, ulong *gicrp)
{
	int node, parent, bus, addr_cells, size_cells, len;
	const fdt32_t *reg;

	if (!blob || fdt_check_header(blob))
		return -FDT_ERR_BADMAGIC;
	node = fdt_node_offset_by_compatible(blob, -1, "arm,gic-v3");
	if (node < 0)
		return node;
	parent = fdt_parent_offset(blob, node);
	for (bus = parent; bus > 0; bus = fdt_parent_offset(blob, bus)) {
		if (!fdt_getprop(blob, bus, "ranges", &len) || len)
			return -FDT_ERR_BADVALUE;
	}
	if (bus < 0)
		return bus;

	addr_cells = fdt_address_cells(blob, parent);
	size_cells = fdt_size_cells(blob, parent);
	reg = fdt_getprop(blob, node, "reg", &len);
	if (!reg || len < 2 * (addr_cells + size_cells) * sizeof(*reg))
		return -FDT_ERR_BADVALUE;
	*gicdp = fdt_read_number(reg, addr_cells);
	*gicrp = fdt_read_number(reg + addr_cells + size_cells, addr_cells);

	return 0;
}

/**
 * gicv3_find() - Find the GICv3
 *
 * @blob: Devicetree to look in, if MON uses one
 * @gicdp: Returns the address of the distributor
 * @gicrp: Returns the address of the redistributors
 * Return: 0 if OK, -ve FDT_ERR_... value if it cannot be found
 */
static int gicv3_find(const void *blob, ulong *gicdp, ulong *gicrp)
{
	if (IS_ENABLED(CONFIG_MON_FDT))
		return gicv3_find_fdt(blob, gicdp, gicrp);

	*gicdp = IF_ENABLED_INT(CONFIG_MON_BARE, CONFIG_MON_GICD_BASE);
	*gicrp = IF_ENABLED_INT(CONFIG_MON_BARE, CONFIG_MON_GICR_BASE);

	return 0;
}

/**
 * gicv3_init_secure() - Hand the interrupt controller to the non-secure side
 *
 * This puts all interrupts in Group 1 non-secure and lets the lower exception
 * levels use the GIC's system registers. It is based on gic_init_secure() and
 * gic_init_secure_percpu() in arch/arm/lib/gic_64.S
 *
 * @blob: Devicetree to find the interrupt controller in, if MON uses one
 * Return: 0 if OK, -ve FDT_ERR_... value if it cannot be found
 */
static int gicv3_init_secure(const void *blob)
{
	ulong mpidr = sysreg_read(mpidr_el1);
	u32 aff = (mpidr & MPIDR_AFF_LOW_MASK) |
		  ((mpidr >> MPIDR_AFF3_SHIFT & MPIDR_AFF3_MASK) << 24);
	void __iomem *gicd, *gicr, *sgi;
	ulong gicd_base, gicr_base;
	int i, lines, ret;

	ret = gicv3_find(blob, &gicd_base, &gicr_base);
	if (ret)
		return ret;
	gicd = (void __iomem *)gicd_base;
	gicr = (void __iomem *)gicr_base;

	writel(GICD_CTLR_ENABLE_G0 | GICD_CTLR_ENABLE_G1NS |
	       GICD_CTLR_ENABLE_G1S | GICD_CTLR_ARE_S | GICD_CTLR_ARE_NS,
	       gicd + GICD_CTLR);
	lines = readl(gicd + GICD_TYPER) & GICD_TYPER_ITLINES_MASK;
	for (i = 1; i <= lines; i++) {
		writel(~0, gicd + GICD_IGROUPRn + i * 4);
		writel(0, gicd + GICD_IGROUPMODRn + i * 4);
	}

	/* Find this CPU's redistributor and wake it */
	while (readq(gicr + GICR_TYPER) >> GICR_TYPER_AFF_SHIFT != aff)
		gicr += GICR_SIZE;
	clrbits_le32(gicr + GICR_WAKER, GICR_WAKER_PROCESSORSLEEP);
	while (readl(gicr + GICR_WAKER) & GICR_WAKER_CHILDRENASLEEP)
		;

	/* SGIs and PPIs are Group 1 non-secure too */
	sgi = gicr + GICR_FRAME_SIZE;
	writel(~0, sgi + GICR_IGROUPRn);
	writel(0, sgi + GICR_IGROUPMODRn);

	/* Use system registers, with EL2 and EL1 allowed to do so too */
	sysreg_write(ICC_SRE_EL3, sysreg_read(ICC_SRE_EL3) | ICC_SRE_ALL);
	isb();
	sysreg_write(ICC_IGRPEN1_EL3, ICC_IGRPEN1_G1NS | ICC_IGRPEN1_G1S);
	sysreg_write(ICC_CTLR_EL3, 0);
	sysreg_write(ICC_SRE_EL2, sysreg_read(ICC_SRE_EL2) | ICC_SRE_ALL);
	isb();

	return 0;
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
	int ret;

	/* Only EL3 can tell the lower levels how fast the timer runs */
	if (CONFIG_COUNTER_FREQUENCY)
		sysreg_write(cntfrq_el0, CONFIG_COUNTER_FREQUENCY);
	ret = gicv3_init_secure(blob);
	mon_board_init();

	/*
	 * With nothing to start, or no interrupt controller for it to use,
	 * wait for an SMC which will never come
	 */
	while (!bl33 || ret)
		;

	/* This starts EL2 with exceptions masked, which is what SPL asks for */
	armv8_switch_to_el2(bl33->args.arg0, 0, 0, 0, bl33->pc, ES_TO_AARCH64);
}
