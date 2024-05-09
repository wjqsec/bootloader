/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __MACH_XBURST_DEVICES_H
#define __MACH_XBURST_DEVICES_H

#include <driver.h>

struct device *jz_add_uart(int id, unsigned long base, unsigned int clock);

#endif /* __MACH_XBURST_DEVICES_H */
