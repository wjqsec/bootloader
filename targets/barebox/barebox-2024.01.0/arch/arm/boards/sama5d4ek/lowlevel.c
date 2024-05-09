/*
 * Copyright (C) 2009-2013 Jean-Christophe PLAGNIOL-VILLARD <plagnioj@jcrosoft.com>
 *
 * Under GPLv2
 */

#include <common.h>
#include <init.h>

#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>

#include <mach/at91/at91_ddrsdrc.h>
#include <mach/at91/hardware.h>

void __naked __bare_init barebox_arm_reset_vector(uint32_t r0, uint32_t r1, uint32_t r2)
{
	arm_cpu_lowlevel_init();

	arm_setup_stack(SAMA5D4_SRAM_BASE + SAMA5D4_SRAM_SIZE);

	barebox_arm_entry(SAMA5_DDRCS, at91sama5d4_get_ddram_size(), NULL);
}
