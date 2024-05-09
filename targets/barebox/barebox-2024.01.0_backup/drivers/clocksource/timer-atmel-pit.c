// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Alex Zuepke <azu@sysgo.de>
 *
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <gj@denx.de>
 */

#include <common.h>
#include <init.h>
#include <clock.h>
#include <mach/at91/hardware.h>
#include <mach/at91/at91_pit.h>
#include <io.h>
#include <linux/clk.h>
#include <linux/err.h>

#define PIT_CPIV(x)	((x) & AT91_PIT_CPIV)
#define pit_write(reg, val)	writel(val, pit_base + reg)
#define pit_read(reg)		readl(pit_base + reg)

static __iomem void *pit_base;

static uint64_t at91sam9_clocksource_read(void)
{
	return pit_read(AT91_PIT_PIIR);
}

static struct clocksource cs = {
	.read	= at91sam9_clocksource_read,
	.mask	= CLOCKSOURCE_MASK(32),
	.shift	= 10,
	.priority = 70,
};

static void at91_pit_stop(void)
{
	/* Disable timer and irqs */
	pit_write(AT91_PIT_MR, 0);

	/* Clear any pending interrupts, wait for PIT to stop counting */
	while (PIT_CPIV(pit_read(AT91_PIT_PIVR)) != 0);
}

static void at91sam926x_pit_reset(void)
{
	at91_pit_stop();

	/* Start PIT but don't enable IRQ */
	pit_write(AT91_PIT_MR, 0xfffff | AT91_PIT_PITEN);
}

static int at91_pit_probe(struct device *dev)
{
	struct clk *clk;
	u32 pit_rate;
	int ret;

	clk = clk_get(dev, NULL);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		dev_err(dev, "clock not found: %d\n", ret);
		return ret;
	}

	ret = clk_enable(clk);
	if (ret < 0) {
		dev_err(dev, "clock failed to enable: %d\n", ret);
		clk_put(clk);
		return ret;
	}

	pit_base = dev_request_mem_region_err_null(dev, 0);
	if (!pit_base)
		return -ENOENT;

	pit_rate = clk_get_rate(clk) / 16;

	at91sam926x_pit_reset();

	cs.mult = clocksource_hz2mult(pit_rate, cs.shift);

	return init_clock(&cs);
}

const static __maybe_unused struct of_device_id at91_pit_dt_ids[] = {
	{
		.compatible = "atmel,at91sam9260-pit",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, at91_pit_dt_ids);

static struct driver at91_pit_driver = {
	.name = "at91-pit",
	.probe = at91_pit_probe,
	.of_compatible = DRV_OF_COMPAT(at91_pit_dt_ids),
};

postcore_platform_driver(at91_pit_driver);
