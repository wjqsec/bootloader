// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2018 Grinn

/* Author: Marcin Niestroj <m.niestroj@grinn-global.com> */

#include <debug_ll.h>
#include <mach/imx/debug_ll.h>
#include <common.h>
#include <linux/sizes.h>
#include <io.h>
#include <image-metadata.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <asm/sections.h>
#include <asm/cache.h>
#include <asm/mmu.h>
#include <mach/imx/esdctl.h>
#include <mach/imx/imx6.h>

static inline void setup_uart(void)
{
	void __iomem *iomuxbase = IOMEM(MX6_IOMUXC_BASE_ADDR);
	void __iomem *uart = IOMEM(MX6_UART1_BASE_ADDR);

	relocate_to_current_adr();
	setup_c();
	barrier();

	imx6_ungate_all_peripherals();

	writel(0x0, iomuxbase + 0x84);
	writel(0x1b0b1, iomuxbase + 0x0310);

	imx6_uart_setup(uart);

	pbl_set_putc(imx_uart_putc, uart);

	putchar('>');
}

BAREBOX_IMD_TAG_STRING(liteboard_memsize_SZ_256M, IMD_TYPE_PARAMETER,
		"memsize=256", 0);
BAREBOX_IMD_TAG_STRING(liteboard_memsize_SZ_512M, IMD_TYPE_PARAMETER,
		"memsize=512", 0);

extern char __dtb_imx6ul_liteboard_start[];

static void __noreturn start_imx6_liteboard(void)
{
	imx6ul_cpu_lowlevel_init();

	arm_setup_stack(0x00910000);

	arm_early_mmu_cache_invalidate();

	if (IS_ENABLED(CONFIG_PBL_CONSOLE))
		setup_uart();

	imx6ul_barebox_entry(__dtb_imx6ul_liteboard_start +
			get_runtime_offset());
}

#define LITEBOARD_ENTRY(name, memory_size)				\
	ENTRY_FUNCTION(name, r0, r1, r2)				\
	{								\
		IMD_USED(liteboard_memsize_##memory_size);		\
									\
		start_imx6_liteboard();					\
	}


LITEBOARD_ENTRY(start_imx6ul_liteboard_256mb, SZ_256M);
LITEBOARD_ENTRY(start_imx6ul_liteboard_512mb, SZ_512M);
