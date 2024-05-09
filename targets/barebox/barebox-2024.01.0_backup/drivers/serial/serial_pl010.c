// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2000 Rob Taylor <robt@flyingpig.com>, Flying Pig Systems
// SPDX-FileCopyrightText: 2004 ARM Ltd.
// SPDX-FileCopyrightText: 2010 Matthias Kaehlcke <matthias@kaehlcke.net>

/* Contributor: Philippe Robin <philippe.robin@arm.com> */

/* Simple U-Boot driver for the PrimeCell PL010/PL011 UARTs */

#include <common.h>
#include <init.h>
#include <malloc.h>
#include <io.h>
#include "serial_pl010.h"

static int pl010_setbaudrate(struct console_device *cdev, int baudrate)
{
	struct pl010_struct *pl010 = cdev->dev->priv;
	unsigned int divisor;

	switch (baudrate) {
	case 9600:
		divisor = UART_PL010_BAUD_9600;
		break;

	case 19200:
		divisor = UART_PL010_BAUD_9600;
		break;

	case 38400:
		divisor = UART_PL010_BAUD_38400;
		break;

	case 57600:
		divisor = UART_PL010_BAUD_57600;
		break;

	case 115200:
		divisor = UART_PL010_BAUD_115200;
		break;

	default:
		divisor = UART_PL010_BAUD_38400;
	}

	writel((divisor & 0xf00) >> 8, &pl010->linctrlmid);
	writel(divisor & 0xff, &pl010->linctrllow);

	/* high register must always be written */
	writel(readl(&pl010->linctrlhigh), &pl010->linctrlhigh);

	return 0;
}

static int pl010_init_port(struct console_device *cdev)
{
	struct pl010_struct *pl010 = cdev->dev->priv;

	/*
	 * First, disable everything.
	 */
	writel(0x00, &pl010->ctrl);

	/*
	 * Set the UART to be 8 bits, 1 stop bit, no parity, fifo enabled.
	 */
	writel(UART_PL010_LCRH_WLEN_8 | UART_PL010_LCRH_FEN,
		&pl010->linctrlhigh);

	/*
	 * Finally, enable the UART
	 */
	writel(UART_PL010_CR_UARTEN, &pl010->ctrl);

	return 0;
}

static void pl010_putc(struct console_device *cdev, char c)
{
	struct pl010_struct *pl010 = cdev->dev->priv;

	/* Wait until there is space in the FIFO */
	while (readl(&pl010->flag) & UART_PL010_FR_TXFF)
		; /* noop */

	/* Send the character */
	writel(c, &pl010->data);
}

static int pl010_getc(struct console_device *cdev)
{
	struct pl010_struct *pl010 = cdev->dev->priv;
	unsigned int data;

	/* Wait until there is data in the FIFO */
	while (readl(&pl010->flag) & UART_PL010_FR_RXFE)
		; /* noop */

	data = readl(&pl010->data);

	/* Check for an error flag */
	if (data & 0xFFFFFF00) {
		/* Clear the error */
		writel(0xFFFFFFFF, &pl010->errclr);
		return -1;
	}

	return (int)data;
}

static int pl010_tstc(struct console_device *cdev)
{
	struct pl010_struct *pl010 = cdev->dev->priv;

	return !(readl(&pl010->flag) & UART_PL010_FR_RXFE);
}

static int pl010_probe(struct device *dev)
{
	struct resource *iores;
	struct console_device *cdev;

	cdev = xzalloc(sizeof(struct console_device));
	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);
	dev->priv = IOMEM(iores->start);
	cdev->dev = dev;
	cdev->tstc = pl010_tstc;
	cdev->putc = pl010_putc;
	cdev->getc = pl010_getc;
	cdev->setbrg = pl010_setbaudrate;

	pl010_init_port(cdev);

	console_register(cdev);

	return 0;
}

static struct driver pl010_driver = {
	.name  = "pl010_serial",
	.probe = pl010_probe,
};
console_platform_driver(pl010_driver);
