// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * pwm LED support for barebox
 *
 * (C) Copyright 2010 Sascha Hauer, Pengutronix
 */
#include <common.h>
#include <malloc.h>
#include <init.h>
#include <led.h>
#include <pwm.h>
#include <of.h>
#include <linux/math64.h>

struct pwmled {
	bool active_low;
	struct led led;
	struct pwm_device *pwm;
};

static void led_pwm_set(struct led *led, unsigned int brightness)
{
	struct pwmled *pwmled = container_of(led, struct pwmled, led);
	unsigned long long duty;
	struct pwm_state state;
	unsigned int max = pwmled->led.max_value;

	pwm_get_state(pwmled->pwm, &state);

	duty = state.period_ns * brightness;
        do_div(duty, max);

	if (pwmled->active_low)
		duty = state.period_ns - duty;

	state.p_enable = true;
	state.duty_ns = duty;

	pwm_apply_state(pwmled->pwm, &state);
}

static int led_pwm_of_probe(struct device *dev)
{
	struct device_node *child;
	int ret;

	for_each_child_of_node(dev->of_node, child) {
		struct pwmled *pwmled;
		struct pwm_device *pwm;

		pwm = of_pwm_request(child, NULL);
		if (IS_ERR(pwm))
			continue;

		pwmled = xzalloc(sizeof(*pwmled));
		pwmled->led.name = xstrdup(child->name);
		pwmled->pwm = pwm;

		ret = of_property_read_u32(child, "max-brightness", &pwmled->led.max_value);
		if (ret)
			return ret;

		pwmled->active_low = of_property_read_bool(child, "active-low");

		pwmled->led.set = led_pwm_set;

		led_pwm_set(&pwmled->led, 0);

		ret = led_register(&pwmled->led);
		if (ret)
			return ret;

		led_of_parse_trigger(&pwmled->led, child);
	}

	return 0;
}

static struct of_device_id led_pwm_of_ids[] = {
	{ .compatible = "pwm-leds", },
	{ }
};
MODULE_DEVICE_TABLE(of, led_pwm_of_ids);

static struct driver led_pwm_of_driver = {
	.name  = "pwm-leds",
	.probe = led_pwm_of_probe,
	.of_compatible = DRV_OF_COMPAT(led_pwm_of_ids),
};
device_platform_driver(led_pwm_of_driver);
