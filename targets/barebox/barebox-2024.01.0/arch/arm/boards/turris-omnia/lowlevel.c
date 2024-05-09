// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2017 Uwe Kleine-Koenig <u.kleine-koenig@pengutronix.de>, Pengutronix

#include <common.h>
#include <asm/barebox-arm.h>
#include <mach/mvebu/barebox-arm-head.h>
#include <mach/mvebu/lowlevel.h>

#include <asm/io.h>

extern char __dtb_armada_385_turris_omnia_bb_start[];

ENTRY_FUNCTION_MVEBU(start_turris_omnia, r0, r1, r2)
{
	void *fdt;

	arm_cpu_lowlevel_init();

	fdt = __dtb_armada_385_turris_omnia_bb_start +
		get_runtime_offset();

	armada_370_xp_barebox_entry(fdt);
}
