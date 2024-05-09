// SPDX-License-Identifier: GPL-2.0-only
/*
 * linux_console.c - Use stdin/stdout as a console device
 *
 * Copyright (c) 2007 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 */

#include <common.h>
#include <driver.h>
#include <init.h>
#include <mach/linux.h>
#include <malloc.h>
#include <console.h>
#include <xfuncs.h>

static void linux_console_putc(struct console_device *cdev, char c)
{
	struct device *dev = cdev->dev;
	struct linux_console_data *d = dev->platform_data;

	linux_write(d->stdoutfd, &c, 1);
}

static int linux_console_tstc(struct console_device *cdev)
{
	struct device *dev = cdev->dev;
	struct linux_console_data *d = dev->platform_data;

	return linux_tstc(d->stdinfd);
}

static int linux_console_getc(struct console_device *cdev)
{
	struct device *dev = cdev->dev;
	struct linux_console_data *d = dev->platform_data;
	static char old_c;
	char c;

	linux_read(d->stdinfd, &c, 1);

	if (old_c == 0x1c && c == 'q')
		panic("^\\q pressed - exiting");

	old_c = c;
	return c;
}

static int linux_console_probe(struct device *dev)
{
	struct console_device *cdev;
	struct linux_console_data *data = dev->platform_data;

	cdev = xzalloc(sizeof(struct console_device));
	cdev->dev = dev;
	if (data->stdinfd >= 0) {
		cdev->tstc = linux_console_tstc;
		cdev->getc = linux_console_getc;
	}
	if (data->stdoutfd >= 0)
		cdev->putc = linux_console_putc;

	console_register(cdev);

	pr_info("%s: registered as %s%d\n", dev->name, cdev->class_dev.name,
		cdev->class_dev.id);

	return 0;
}

static struct driver linux_console_driver = {
        .name  = "console",
        .probe = linux_console_probe,
};
console_platform_driver(linux_console_driver);
