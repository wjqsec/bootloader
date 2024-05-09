/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __INPUT_KEYBOARD_H
#define __INPUT_KEYBOARD_H

#include <linux/types.h>

#define NR_KEYS	256

extern uint8_t keycode_bb_keys[NR_KEYS];
extern uint8_t keycode_bb_shift_keys[NR_KEYS];

#endif
