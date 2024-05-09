/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __MACH_OMAP3_GENERIC_H
#define __MACH_OMAP3_GENERIC_H

#include <linux/sizes.h>
#include <linux/string.h>
#include <mach/omap/generic.h>
#include <mach/omap/omap3-silicon.h>

static inline void omap3_save_bootinfo(uint32_t *info)
{
	unsigned long i = (unsigned long)info;

	if (i & 0x3)
		return;
	if (i < OMAP3_SRAM_BASE)
		return;
	if (i > OMAP3_SRAM_BASE + SZ_64K)
		return;

	memcpy((void *)OMAP3_SRAM_SCRATCH_SPACE, info, 3 * sizeof(uint32_t));
}

u32 omap3_running_in_flash(void);
u32 omap3_running_in_sram(void);
u32 omap3_running_in_sdram(void);

int omap3_init(void);
int omap3_devices_init(void);

void *omap3_xload_boot_usb(void);

#endif /* __MACH_OMAP3_GENERIC_H */
