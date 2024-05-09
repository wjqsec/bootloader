/*
 * Copyright (C) 2009-2013 Jean-Christophe PLAGNIOL-VILLARD <plagnioj@jcrosoft.com>
 *
 * Under GPLv2
 */

#include <common.h>
#include <init.h>

#include <mach/at91/barebox-arm.h>
#include <mach/at91/at91sam9_sdramc.h>
#include <mach/at91/at91sam9260.h>
#include <mach/at91/hardware.h>

AT91_ENTRY_FUNCTION(start_dss11, r0, r1, r2)
{
	arm_cpu_lowlevel_init();

	arm_setup_stack(AT91SAM9260_SRAM_BASE + AT91SAM9260_SRAM_SIZE);

	barebox_arm_entry(AT91_CHIPSELECT_1,
			  at91_get_sdram_size(IOMEM(AT91SAM9260_BASE_SDRAMC)),
			  NULL);
}
