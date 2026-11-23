/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Interface between MON, the secure monitor, and U-Boot proper
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#ifndef __ASM_MON_H
#define __ASM_MON_H

/*
 * Offset from the start of MON of the entry point which puts MON back in
 * charge of EL3. This is called at EL3 and returns, with only x9 and x10
 * changed
 */
#define MON_RECLAIM_OFFSET	4

/* Address of that entry point */
#define MON_RECLAIM_ADDR	(CONFIG_MON_TEXT_BASE + MON_RECLAIM_OFFSET)

#endif
