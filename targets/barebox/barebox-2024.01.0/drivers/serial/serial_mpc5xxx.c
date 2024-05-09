// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * (C) Copyright 2000 - 2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * Hacked for MPC8260 by Murray.Jensen@cmst.csiro.au, 19-Oct-00, with
 * changes based on the file arch/ppc/mbxboot/m8260_tty.c from the
 * Linux/PPC sources (m8260_tty.c had no copyright info in it).
 *
 * Martin Krause, 8 Jun 2006
 * Added CONFIG_SERIAL_MULTI support
 */

/*
 * Minimal serial functions needed to use one of the PSC ports
 * as serial console interface.
 */

#include <common.h>
#include <mach/mpc5xxx.h>
#include <driver.h>
#include <init.h>
#include <console.h>
#include <xfuncs.h>
#include <mach/clock.h>
#include <linux/err.h>

static int __mpc5xxx_serial_setbaudrate(struct mpc5xxx_psc *psc, int baudrate)
{
	unsigned long baseclk;
	int div;

#if defined(CONFIG_MGT5100)
	baseclk = (CFG_MPC5XXX_CLKIN + 16) / 32;
#elif defined(CONFIG_MPC5200)
	baseclk = (get_ipb_clock() + 16) / 32;
#endif

	/* set up UART divisor */
	div = (baseclk + (baudrate/2)) / baudrate;
	psc->ctur = (div >> 8) & 0xFF;
	psc->ctlr =  div & 0xff;

	return 0;
}

static int mpc5xxx_serial_setbaudrate(struct console_device *cdev, int baudrate)
{
	struct device *dev = cdev->dev;
	struct mpc5xxx_psc *psc = dev->priv;

	__mpc5xxx_serial_setbaudrate(psc, baudrate);

	return 0;
}

static int __mpc5xxx_serial_init(struct mpc5xxx_psc *psc)
{
	/* reset PSC */
	psc->command = PSC_SEL_MODE_REG_1;

	/* select clock sources */
#if defined(CONFIG_MGT5100)
	psc->psc_clock_select = 0xdd00;
#elif defined(CONFIG_MPC5200)
	psc->psc_clock_select = 0;
#endif

	/* switch to UART mode */
	psc->sicr = 0;

	/* configure parity, bit length and so on */
#if defined(CONFIG_MGT5100)
	psc->mode = PSC_MODE_ERR | PSC_MODE_8_BITS | PSC_MODE_PARNONE;
#elif defined(CONFIG_MPC5200)
	psc->mode = PSC_MODE_8_BITS | PSC_MODE_PARNONE;
#endif
	psc->mode = PSC_MODE_ONE_STOP;

	/* disable all interrupts */
	psc->psc_imr = 0;

	/* reset and enable Rx/Tx */
//	psc->command = PSC_RST_RX;
//	psc->command = PSC_RST_TX;
	psc->command = PSC_RX_ENABLE | PSC_TX_ENABLE;

	return 0;
}

static int mpc5xxx_serial_init(struct console_device *cdev)
{
	struct device *dev = cdev->dev;
	struct mpc5xxx_psc *psc = dev->priv;

	__mpc5xxx_serial_init(psc);

	return 0;
}

static void mpc5xxx_serial_putc (struct console_device *cdev, const char c)
{
	struct device *dev = cdev->dev;
	struct mpc5xxx_psc *psc = dev->priv;

	/* Wait for last character to go. */
	while (!(psc->psc_status & PSC_SR_TXEMP))
		;

	psc->psc_buffer_8 = c;
}

static int mpc5xxx_serial_getc (struct console_device *cdev)
{
	struct device *dev = cdev->dev;
	struct mpc5xxx_psc *psc = dev->priv;

	/* Wait for a character to arrive. */
	while (!(psc->psc_status & PSC_SR_RXRDY))
		;

	return psc->psc_buffer_8;
}

static int mpc5xxx_serial_tstc (struct console_device *cdev)
{
	struct device *dev = cdev->dev;
	struct mpc5xxx_psc *psc = dev->priv;

	return (psc->psc_status & PSC_SR_RXRDY);
}

static int mpc5xxx_serial_probe(struct device *dev)
{
	struct resource *iores;
	struct console_device *cdev;

	cdev = xzalloc(sizeof(struct console_device));
	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);
	dev->priv = IOMEM(iores->start);
	cdev->dev = dev;
	cdev->tstc = mpc5xxx_serial_tstc;
	cdev->putc = mpc5xxx_serial_putc;
	cdev->getc = mpc5xxx_serial_getc;
	cdev->setbrg = mpc5xxx_serial_setbaudrate;

	mpc5xxx_serial_init(cdev);

	console_register(cdev);

	return 0;
}

static struct driver mpc5xxx_serial_driver = {
        .name  = "mpc5xxx_serial",
        .probe = mpc5xxx_serial_probe,
};
console_platform_driver(mpc5xxx_serial_driver);
