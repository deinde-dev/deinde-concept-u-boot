// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2003 Josef Baumgartner <josef.baumgartner@telex.de>
 *
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <init.h>
#include <irq_func.h>
#include <time.h>
#include <asm/global_data.h>
#include <linux/delay.h>

#include <asm/timer.h>
#include <asm/immap.h>
#include <watchdog.h>

DECLARE_GLOBAL_DATA_PTR;

static volatile ulong timestamp = 0;

#ifndef CFG_SYS_WATCHDOG_FREQ
#define CFG_SYS_WATCHDOG_FREQ (CONFIG_SYS_HZ / 2)
#endif

#if IS_ENABLED(CONFIG_MCFTMR)
#ifndef CFG_SYS_UDELAY_BASE
#	error	"uDelay base not defined!"
#endif

#if !defined(CFG_SYS_TMR_BASE) || !defined(CFG_SYS_INTR_BASE) || !defined(CFG_SYS_TMRINTR_NO) || !defined(CFG_SYS_TMRINTR_MASK)
#	error	"TMR_BASE, INTR_BASE, TMRINTR_NO or TMRINTR_MASk not defined!"
#endif
extern void dtimer_intr_setup(void);

void __udelay(unsigned long usec)
{
	volatile dtmr_t *timerp = (dtmr_t *) (CFG_SYS_UDELAY_BASE);
	uint start, now, tmp;

	while (usec > 0) {
		if (usec > 65000)
			tmp = 65000;
		else
			tmp = usec;
		usec = usec - tmp;

		/* Set up TIMER 3 as timebase clock */
		timerp->tmr = DTIM_DTMR_RST_RST;
		timerp->tcn = 0;
		/* set period to 1 us */
		timerp->tmr =
		    CFG_SYS_TIMER_PRESCALER | DTIM_DTMR_CLK_DIV1 | DTIM_DTMR_FRR |
		    DTIM_DTMR_RST_EN;

		start = now = timerp->tcn;
		while (now < start + tmp)
			now = timerp->tcn;
	}
}

void dtimer_interrupt(void *not_used)
{
	volatile dtmr_t *timerp = (dtmr_t *) (CFG_SYS_TMR_BASE);

	/* check for timer interrupt asserted */
	if ((CFG_SYS_TMRPND_REG & CFG_SYS_TMRINTR_MASK) == CFG_SYS_TMRINTR_PEND) {
		timerp->ter = (DTIM_DTER_CAP | DTIM_DTER_REF);
		timestamp++;

		#if defined(CONFIG_WATCHDOG) || defined (CONFIG_HW_WATCHDOG)
		if (CFG_SYS_WATCHDOG_FREQ && (timestamp % (CFG_SYS_WATCHDOG_FREQ)) == 0) {
			schedule();
		}
		#endif    /* CONFIG_WATCHDOG || CONFIG_HW_WATCHDOG */
		return;
	}
}

int timer_init(void)
{
	volatile dtmr_t *timerp = (dtmr_t *) (CFG_SYS_TMR_BASE);

	timestamp = 0;

	timerp->tcn = 0;
	timerp->trr = 0;

	/* Set up TIMER 4 as clock */
	timerp->tmr = DTIM_DTMR_RST_RST;

	/* initialize and enable timer interrupt */
	irq_install_handler(CFG_SYS_TMRINTR_NO, dtimer_interrupt, 0);

	timerp->tcn = 0;
	timerp->trr = 1000;	/* Interrupt every ms */

	dtimer_intr_setup();

	/* set a period of 1us, set timer mode to restart and enable timer and interrupt */
	timerp->tmr = CFG_SYS_TIMER_PRESCALER | DTIM_DTMR_CLK_DIV1 |
	    DTIM_DTMR_FRR | DTIM_DTMR_ORRI | DTIM_DTMR_RST_EN;

	return 0;
}

ulong get_timer(ulong base)
{
	return (timestamp - base);
}

/*
 * This function is derived from PowerPC code (read timebase as long long).
 * On M68K it just returns the timer value.
 */
unsigned long long get_ticks(void)
{
	return get_timer(0);
}
#else
/*
 * The PIT is a 16-bit counter which counts down and wraps. The number of ticks
 * so far is kept in gd->timebase_h and gd->timebase_l. Its low 16 bits follow
 * the counter, so the ticks since the last call can be worked out from them.
 *
 * This cannot be kept in a variable. Before relocation the data section is in
 * flash, so a write to it is lost
 */
uint64_t __weak get_ticks(void)
{
	volatile pit_t *timerp = (pit_t *) (CFG_SYS_UDELAY_BASE);
	u16 val = ~timerp->pcntr;
	u64 ticks = (u64)gd->timebase_h << 32 | gd->timebase_l;

	/* the counter has 0x10000 values, so 16-bit arithmetic handles a wrap */
	ticks += (u16)(val - (u16)ticks);
	gd->timebase_h = ticks >> 32;
	gd->timebase_l = ticks;

	return ticks;
}

/* PIT timer */
int timer_init(void)
{
	volatile pit_t *timerp = (pit_t *) (CFG_SYS_UDELAY_BASE);

	gd->timebase_h = 0;
	gd->timebase_l = 0;

	/* Set up PIT as timebase clock */
	timerp->pmr = 0xffff;
	timerp->pcsr = PIT_PCSR_EN | PIT_PCSR_OVW;

	return 0;
}
#endif				/* CONFIG_MCFTMR */

unsigned long usec2ticks(unsigned long usec)
{
	return usec_to_tick(usec);
}

/*
 * This function is derived from PowerPC code (timebase clock frequency).
 * On M68K it returns the number of timer ticks per second.
 */
ulong get_tbclk(void)
{
	return CONFIG_SYS_HZ;
}
