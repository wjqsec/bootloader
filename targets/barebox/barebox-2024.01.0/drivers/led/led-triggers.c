// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * LED trigger support for barebox
 *
 * (C) Copyright 2010 Sascha Hauer, Pengutronix
 */

#include <common.h>
#include <poller.h>
#include <errno.h>
#include <clock.h>
#include <led.h>
#include <init.h>

/**
 * @file
 * @brief LED trigger framework
 *
 * This file contains triggers which can be associated to LEDs.
 *
 * With this framework LEDs can be associated to different events.
 * An event can be a heartbeat, network activity or panic.
 * led_trigger() is the central function which is called in the
 * different barebox frameworks to trigger an event.
 *
 * currently there are the following triggers are defined:
 *
 * led_trigger_panic:		triggered in panic()
 * led_trigger_heartbeat:	shows the heartbeat of barebox. Blinks as long
 *				barebox is up and running.
 * led_trigger_net_rx:		Triggered during network packet reception
 * led_trigger_net_tx:		Triggered during network packet transmission
 * led_trigger_net_txrx:	combination of the two above
 */

struct led_trigger_struct {
	struct led *led;
	struct list_head list;
	enum led_trigger trigger;
};

static LIST_HEAD(led_triggers);

/**
 * led_trigger - triggers a trigger
 * @param trigger	The trigger to enable/disable
 * @param enable	true if enable
 *
 * Enable/disable a LED for a given trigger.
 */
void led_trigger(enum led_trigger trigger, enum trigger_type type)
{
	struct led_trigger_struct *led_trigger;

	if (trigger >= LED_TRIGGER_MAX)
		return;

	list_for_each_entry(led_trigger, &led_triggers, list) {
		if (led_trigger->trigger != trigger)
			continue;

		switch (type) {
		case TRIGGER_FLASH:
			led_flash(led_trigger->led, 200);
			break;
		case TRIGGER_ENABLE:
			led_set(led_trigger->led, led_trigger->led->max_value);
			break;
		case TRIGGER_DISABLE:
			led_set(led_trigger->led, 0);
			break;
		}
	}
}

static struct led_trigger_struct *led_find_trigger(struct led *led)
{
	struct led_trigger_struct *led_trigger;

	list_for_each_entry(led_trigger, &led_triggers, list)
		if (led_trigger->led == led)
			return led_trigger;

	return NULL;
}

void led_trigger_disable(struct led *led)
{
	struct led_trigger_struct *led_trigger;

	led_trigger = led_find_trigger(led);
	if (!led_trigger)
		return;

	list_del(&led_trigger->list);

	led_set(led, 0);

	free(led_trigger);
}

/**
 * led_set_trigger - set the LED for a trigger
 * @param trigger	The trigger to set a LED for
 * @param led		The LED
 *
 * This function associates a trigger with a LED. Pass led = NULL
 * to disable a trigger
 */
int led_set_trigger(enum led_trigger trigger, struct led *led)
{
	struct led_trigger_struct *led_trigger;

	if (trigger >= LED_TRIGGER_MAX)
		return -EINVAL;

	led_trigger_disable(led);

	led_trigger = xzalloc(sizeof(*led_trigger));

	led_trigger->led = led;
	led_trigger->trigger = trigger;
	list_add_tail(&led_trigger->list, &led_triggers);

	if (trigger == LED_TRIGGER_DEFAULT_ON)
		led_set(led, led->max_value);
	if (trigger == LED_TRIGGER_HEARTBEAT)
		led_blink(led, 200, 1000);

	return 0;
}

/**
 * led_triggers_show_info - Show information about all registered
 * triggers
 */
void led_triggers_show_info(void)
{
	struct led_trigger_struct *led_trigger;
	int i;

	for (i = 0; i < LED_TRIGGER_MAX; i++) {
		printf("%s", trigger_name(i));

		list_for_each_entry(led_trigger, &led_triggers, list) {
			struct led *led = led_trigger->led;

			if (led_trigger->trigger != i)
				continue;

			printf("\n  LED %d (%s)", led->num, led->name);
		}

		printf("\n");
	}
}
