// SPDX-License-Identifier: GPL-2.0
/*
 * UART driver for MediaTek MT7620 and earlier SoCs
 *
 * Copyright (C) 2020 MediaTek Inc.
 * Author: Weijie Gao <weijie.gao@mediatek.com>
 */

#include <clk.h>
#include <div64.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <reset.h>
#include <serial.h>
#include <watchdog.h>
#include <asm/io.h>
#include <asm/types.h>
#include <dm/device_compat.h>
#include <dm/prop.h>
#include <linux/err.h>

struct mt7620_serial_regs {
	u32 rbr;
	u32 thr;
	u32 ier;
	u32 iir;
	u32 fcr;
	u32 lcr;
	u32 mcr;
	u32 lsr;
	u32 msr;
	u32 scratch;
	u32 dl;
	u32 dll;
	u32 dlm;
	u32 ifctl;
};

#define UART_LCR_WLS_8		0x03	/* 8 bit character length */

#define UART_LSR_DR		0x01	/* Data ready */
#define UART_LSR_THRE		0x20	/* Xmit holding register empty */
#define UART_LSR_TEMT		0x40	/* Xmitter empty */

#define UART_MCR_DTR		0x01	/* DTR */
#define UART_MCR_RTS		0x02	/* RTS */

#define UART_FCR_FIFO_EN	0x01	/* Fifo enable */
#define UART_FCR_RXSR		0x02	/* Receiver soft reset */
#define UART_FCR_TXSR		0x04	/* Transmitter soft reset */

#define UART_MCRVAL (UART_MCR_DTR | \
		     UART_MCR_RTS)

/* Clear & enable FIFOs */
#define UART_FCRVAL (UART_FCR_FIFO_EN | \
		     UART_FCR_RXSR |	\
		     UART_FCR_TXSR)

struct mt7620_serial_plat {
	struct mt7620_serial_regs __iomem *regs;
	u32 clock;
};

static void _mt7620_serial_setbrg(struct mt7620_serial_plat *plat, int baud)
{
	u32 quot;

	/* set divisor */
	quot = DIV_ROUND_CLOSEST(plat->clock, 16 * baud);
	writel(quot, &plat->regs->dl);

	/* set character length and stop bits */
	writel(UART_LCR_WLS_8, &plat->regs->lcr);
}

static int mt7620_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct mt7620_serial_plat *plat = dev_get_plat(dev);

	_mt7620_serial_setbrg(plat, baudrate);

	return 0;
}

static int mt7620_serial_putc(struct udevice *dev, const char ch)
{
	struct mt7620_serial_plat *plat = dev_get_plat(dev);

	if (!(readl(&plat->regs->lsr) & UART_LSR_THRE))
		return -EAGAIN;

	writel(ch, &plat->regs->thr);

	if (ch == '\n')
		schedule();

	return 0;
}

static int mt7620_serial_getc(struct udevice *dev)
{
	struct mt7620_serial_plat *plat = dev_get_plat(dev);

	if (!(readl(&plat->regs->lsr) & UART_LSR_DR))
		return -EAGAIN;

	return readl(&plat->regs->rbr);
}

static int mt7620_serial_pending(struct udevice *dev, bool input)
{
	struct mt7620_serial_plat *plat = dev_get_plat(dev);

	if (input)
		return (readl(&plat->regs->lsr) & UART_LSR_DR) ? 1 : 0;

	return (readl(&plat->regs->lsr) & UART_LSR_THRE) ? 0 : 1;
}

static int mt7620_serial_probe(struct udevice *dev)
{
	struct mt7620_serial_plat *plat = dev_get_plat(dev);

	/* Disable interrupt */
	writel(0, &plat->regs->ier);

	writel(UART_MCRVAL, &plat->regs->mcr);
	writel(UART_FCRVAL, &plat->regs->fcr);

	return 0;
}

static int __maybe_unused mt7620_serial_of_to_plat(struct udevice *dev)
{
	struct mt7620_serial_plat *plat = dev_get_plat(dev);
	DTPLAT(serial_mt7620, dt, dev);
	struct reset_ctl reset_uart;
	struct clk clk;
	fdt_addr_t addr;
	int err;

	/* of-platdata does not support resets */
	if (IS_ENABLED(CONFIG_OF_REAL)) {
		err = reset_get_by_index(dev, 0, &reset_uart);
		if (!err)
			reset_deassert(&reset_uart);
	}

	addr = dev_prop_addr(dev, dt);
	if (addr == FDT_ADDR_T_NONE) {
		dev_err(dev, "mt7620_serial: unable to map UART registers\n");
		return -EINVAL;
	}
	plat->regs = map_physmem(addr, 0, MAP_NOCACHE);

	err = dev_prop_clk(dev, dt, clocks, "clocks", 0, &clk);
	if (!err) {
		err = clk_get_rate(&clk);
		if (!IS_ERR_VALUE(err))
			plat->clock = err;
	} else if (err != -ENOENT && err != -ENODEV && err != -ENOSYS) {
		dev_err(dev, "mt7620_serial: failed to get clock\n");
		return err;
	}

	if (!plat->clock)
		plat->clock = dev_prop_u32_default(dev, dt, clock_frequency,
						   "clock-frequency", 0);

	if (!plat->clock) {
		dev_err(dev, "mt7620_serial: clock not defined\n");
		return -EINVAL;
	}

	return 0;
}

static __maybe_unused const struct udevice_id mt7620_serial_ids[] = {
	{ .compatible = "mediatek,mt7620-uart" },
	{ }
};

static const struct dm_serial_ops mt7620_serial_ops = {
	.putc = mt7620_serial_putc,
	.pending = mt7620_serial_pending,
	.getc = mt7620_serial_getc,
	.setbrg = mt7620_serial_setbrg,
};

U_BOOT_DRIVER(serial_mt7620) = {
	.name = "serial_mt7620",
	.id = UCLASS_SERIAL,
	.of_match = of_match_ptr(mt7620_serial_ids),
	.of_to_plat = of_to_plat_ptr(mt7620_serial_of_to_plat),
	.plat_auto = sizeof(struct mt7620_serial_plat),
	.probe = mt7620_serial_probe,
	.ops = &mt7620_serial_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

#ifdef CONFIG_DEBUG_UART_MT7620

#include <debug_uart.h>

static inline void _debug_uart_init(void)
{
	struct mt7620_serial_plat plat;

	plat.regs = (void *)CONFIG_DEBUG_UART_BASE;
	plat.clock = CONFIG_DEBUG_UART_CLOCK;

	writel(0, &plat.regs->ier);
	writel(UART_MCRVAL, &plat.regs->mcr);
	writel(UART_FCRVAL, &plat.regs->fcr);

	_mt7620_serial_setbrg(&plat, CONFIG_BAUDRATE);
}

static inline void _debug_uart_putc(int ch)
{
	struct mt7620_serial_regs __iomem *regs =
		(void *)CONFIG_DEBUG_UART_BASE;

	while (!(readl(&regs->lsr) & UART_LSR_THRE))
		;

	writel(ch, &regs->thr);
}

DEBUG_UART_FUNCS

#endif
