/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Entry point for MON, the resident phase which runs at EL3 as the secure
 * monitor
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <config.h>
#include <linux/linkage.h>

/*
 * _start() - Start MON
 *
 * SPL jumps here at EL3 as if MON were TF-A's BL31, with the MMU and caches
 * off and exceptions masked
 *
 * x0: TF-A parameters, giving the entry point of U-Boot proper (BL33)
 * x1: Address of the devicetree for U-Boot proper
 */
.section .text.mon_start, "ax"
ENTRY(_start)
	mov	x20, x0
	mov	x21, x1

	/* Use this CPU's secure stack, which is also used for SMCs */
	msr	spsel, #1
	bl	psci_get_cpu_id
	bl	psci_get_cpu_stack_top
	mov	sp, x0

	/* SPL only loads the parts of the image which have contents */
	ldr	x0, =__bss_start
	ldr	x1, =__bss_end
1:	cmp	x0, x1
	b.hs	2f
	str	xzr, [x0], #8
	b	1b

2:	bl	psci_setup_vectors
	mov	x0, x20
	mov	x1, x21
	bl	mon_main
3:	b	3b
ENDPROC(_start)
