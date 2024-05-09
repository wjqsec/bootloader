/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __MACH_MXS_DEBUG_LL_H__
#define __MACH_MXS_DEBUG_LL_H__

#include <io.h>
#include <mach/mxs/imx-regs.h>

#define UARTDBGDR 0x00
#define UARTDBGFR 0x18
# define TXFE (1 << 7)
# define TXFF (1 << 5)

static inline void PUTC_LL(int c)
{
	void __iomem *base = (void *)IMX_DBGUART_BASE;

	/* Wait for room in TX FIFO */
	while (!(readl(base + UARTDBGFR) & TXFE));

	writel(c, base + UARTDBGDR);
}

#endif /* __MACH_MXS_DEBUG_LL_H__ */
