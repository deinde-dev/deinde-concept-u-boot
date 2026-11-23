/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Entry point for MON, the resident phase which runs at EL3 as the secure
 * monitor
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <config.h>
#include <linux/linkage.h>
#include <asm/mon.h>

/*
 * _start() - Start MON
 *
 * SPL jumps here at EL3 as if MON were TF-A's BL31, with the MMU and caches
 * off and exceptions masked
 *
 * x0: TF-A parameters, giving the entry point of U-Boot proper (BL33)
 * x1: Address of the devicetree for U-Boot proper
 *
 * The second instruction is the entry point which U-Boot proper uses to put
 * MON back in charge of EL3
 */
.section .text.mon_start, "ax"
ENTRY(_start)
	b	1f
	. = _start + MON_RECLAIM_OFFSET
	b	mon_reclaim_el3

1:	mov	x20, x0
	mov	x21, x1

	/* Use this CPU's secure stack, which is also used for SMCs */
	msr	spsel, #1
	bl	psci_get_cpu_id
	bl	psci_get_cpu_stack_top
	mov	sp, x0

	/* SPL only loads the parts of the image which have contents */
	ldr	x0, =__bss_start
	ldr	x1, =__bss_end
2:	cmp	x0, x1
	b.hs	3f
	str	xzr, [x0], #8
	b	2b

3:	bl	psci_setup_vectors
	mov	x0, x20
	mov	x1, x21
	bl	mon_main
4:	b	4b
ENDPROC(_start)

/*
 * mon_reclaim_el3() - Put MON back in charge of EL3
 *
 * If U-Boot proper runs at EL3, it uses its own exception vectors there.
 * It calls this, at EL3, just before it drops to EL2, so that SMCs reach MON
 * from then on. The PSCI code selects its stack on each call, so only the
 * vectors need to be set up.
 *
 * Only x9 and x10 are changed, since the caller holds arguments for the OS in
 * the other registers
 */
ENTRY(mon_reclaim_el3)
	mov	x9, x0
	mov	x10, lr
	bl	psci_setup_vectors
	mov	x0, x9
	ret	x10
ENDPROC(mon_reclaim_el3)
