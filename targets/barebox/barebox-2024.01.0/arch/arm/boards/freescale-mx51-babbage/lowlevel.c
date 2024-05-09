// SPDX-License-Identifier: GPL-2.0-only

#include <debug_ll.h>
#include <mach/imx/debug_ll.h>
#include <mach/imx/clock-imx51_53.h>
#include <mach/imx/iomux-mx51.h>
#include <common.h>
#include <mach/imx/esdctl.h>
#include <mach/imx/generic.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>

static inline void setup_uart(void)
{
	void __iomem *iomuxbase = IOMEM(MX51_IOMUXC_BASE_ADDR);
	void __iomem *ccmbase = IOMEM(MX51_CCM_BASE_ADDR);

	/*
	 * Restore CCM values that might be changed by the Mask ROM
	 * code.
	 *
	 * Source: RealView debug scripts provided by Freescale
	 */
	writel(MX5_CCM_CBCDR_RESET_VALUE,  ccmbase + MX5_CCM_CBCDR);
	writel(MX5_CCM_CSCMR1_RESET_VALUE, ccmbase + MX5_CCM_CSCMR1);
	writel(MX5_CCM_CSCDR1_RESET_VALUE, ccmbase + MX5_CCM_CSCDR1);

	imx_setup_pad(iomuxbase, MX51_PAD_UART1_TXD__UART1_TXD);

	imx51_uart_setup_ll();

	putc_ll('>');
}

extern char __dtb_imx51_babbage_start[];

ENTRY_FUNCTION(start_imx51_babbage, r0, r1, r2)
{
	void *fdt;

	imx5_cpu_lowlevel_init();

	if (IS_ENABLED(CONFIG_DEBUG_LL))
		setup_uart();

	arm_setup_stack(0x20000000);

	fdt = __dtb_imx51_babbage_start + get_runtime_offset();

	imx51_barebox_entry(fdt);
}
