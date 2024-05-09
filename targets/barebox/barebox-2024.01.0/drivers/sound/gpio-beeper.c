// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2021, Ahmad Fatoum
 */

#include <common.h>
#include <regulator.h>
#include <sound.h>
#include <of.h>
#include <linux/gpio/consumer.h>

struct gpio_beeper {
	struct gpio_desc *gpio;
	struct sound_card card;
};

static int gpio_beeper_beep(struct sound_card *card, unsigned freq, unsigned duration)
{
	struct gpio_beeper *beeper = container_of(card, struct gpio_beeper, card);

	gpiod_set_value(beeper->gpio, freq);
	return 0;
}

static int gpio_beeper_probe(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct gpio_beeper *beeper;
	struct sound_card *card;
	struct gpio_desc *gpio;

	gpio = gpiod_get(dev, NULL, GPIOD_OUT_LOW);
	if (IS_ERR(gpio))
		return dev_errp_probe(dev, gpio, "failed to request gpio\n");

	beeper = xzalloc(sizeof(*beeper));
	beeper->gpio = gpio;
	dev->priv = beeper;

	card = &beeper->card;
	card->name = np->full_name;
	card->beep = gpio_beeper_beep;

	return sound_card_register(card);
}

static void gpio_beeper_suspend(struct device *dev)
{
	struct gpio_beeper *beeper = dev->priv;

	gpio_beeper_beep(&beeper->card, 0, 0);
}

static const struct of_device_id gpio_beeper_match[] = {
	{ .compatible = "gpio-beeper", },
	{ },
};
MODULE_DEVICE_TABLE(of, gpio_beeper_match);

static struct driver gpio_beeper_driver = {
	.name		= "gpio-beeper",
	.probe		= gpio_beeper_probe,
	.remove		= gpio_beeper_suspend,
	.of_compatible	= gpio_beeper_match,
};
device_platform_driver(gpio_beeper_driver);
