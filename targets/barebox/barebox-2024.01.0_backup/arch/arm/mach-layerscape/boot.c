// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <init.h>
#include <bootsource.h>
#include <mach/layerscape/layerscape.h>
#include <soc/fsl/immap_lsch2.h>

enum bootsource ls1046a_bootsource_get(void)
{
	void __iomem *dcfg = IOMEM(LSCH2_DCFG_ADDR);
	uint32_t rcw_src;

	rcw_src = in_be32(dcfg) >> 23;

	if (rcw_src == 0x40)
		return BOOTSOURCE_MMC;
	if ((rcw_src & 0x1fe) == 0x44)
		return BOOTSOURCE_SPI_NOR;
	if ((rcw_src & 0x1f0) == 0x10)
		/* 8bit NOR Flash */
		return BOOTSOURCE_NOR;
	if ((rcw_src & 0x1f0) == 0x20)
		/* 16bit NOR Flash */
		return BOOTSOURCE_NOR;

	return BOOTSOURCE_UNKNOWN;
}

enum bootsource ls1021a_bootsource_get(void)
{
	return ls1046a_bootsource_get();
}

static int layerscape_bootsource_init(void)
{
	if (of_machine_is_compatible("fsl,ls1046a"))
		bootsource_set_raw(ls1046a_bootsource_get(), BOOTSOURCE_INSTANCE_UNKNOWN);
	if (of_machine_is_compatible("fsl,ls1021a"))
		bootsource_set_raw(ls1021a_bootsource_get(), BOOTSOURCE_INSTANCE_UNKNOWN);

	return 0;
}
coredevice_initcall(layerscape_bootsource_init);
