// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2011 Robert Jarzmik <robert.jarzmik@free.fr>

#include <clock.h>
#include <common.h>
#include <init.h>
#include <poweroff.h>
#include <gpio.h>
#include <poller.h>

#include "mioa701.h"

#define POWEROFF_SECS (4 * SECOND)

static void blink_led_keyboard(void)
{
	gpio_set_value(GPIO115_LED_nKeyboard, 0);
	mdelay(400);
	gpio_set_value(GPIO115_LED_nKeyboard, 1);
	mdelay(400);
}

static void try_poweroff(void)
{
	int poweroff_released = 0;

	blink_led_keyboard();
	poweroff_released |= !gpio_get_value(GPIO0_KEY_POWER);
	if (poweroff_released)
		return;

	gpio_set_value(GPIO115_LED_nKeyboard, 0);
	mdelay(2000);
	poweroff_machine();
}

static void gpio0_poller_fn(struct poller_struct *poller)
{
	static uint64_t gpio0_start;
	static bool gpio0_activated;

	if (!gpio_get_value(GPIO0_KEY_POWER)) {
		gpio0_activated = false;
		return;
	}

	if (gpio0_activated) {
		if (is_timeout_non_interruptible(gpio0_start, POWEROFF_SECS)) {
			try_poweroff();
			gpio0_activated = false;
		}
	} else {
		gpio0_activated = true;
		gpio0_start = get_time_ns();
	}
}

static struct poller_struct gpio0_poller = {
	.func = gpio0_poller_fn,
};

static int gpio0_poweroff_probe(void)
{
	return poller_register(&gpio0_poller, "power-button");
}

device_initcall(gpio0_poweroff_probe);
