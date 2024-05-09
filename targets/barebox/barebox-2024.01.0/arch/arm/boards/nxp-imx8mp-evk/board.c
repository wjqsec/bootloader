// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Oleksij Rempel, Pengutronix
 */

#include <asm/memory.h>
#include <bootsource.h>
#include <common.h>
#include <deep-probe.h>
#include <init.h>
#include <linux/phy.h>
#include <linux/sizes.h>
#include <mach/imx/bbu.h>
#include <mach/imx/iomux-mx8mp.h>
#include <gpio.h>
#include <envfs.h>

static int nxp_imx8mp_evk_probe(struct device *dev)
{
	int emmc_bbu_flag = 0;
	int sd_bbu_flag = 0;
	u32 val;

	if (bootsource_get() == BOOTSOURCE_MMC) {
		if (bootsource_get_instance() == 2) {
			of_device_enable_path("/chosen/environment-emmc");
			emmc_bbu_flag = BBU_HANDLER_FLAG_DEFAULT;
		} else {
			of_device_enable_path("/chosen/environment-sd");
			sd_bbu_flag = BBU_HANDLER_FLAG_DEFAULT;
		}
	} else {
		of_device_enable_path("/chosen/environment-emmc");
		emmc_bbu_flag = BBU_HANDLER_FLAG_DEFAULT;
	}

	imx8m_bbu_internal_mmc_register_handler("SD", "/dev/mmc1.barebox", sd_bbu_flag);
	imx8m_bbu_internal_mmcboot_register_handler("eMMC", "/dev/mmc2", emmc_bbu_flag);
	imx8m_bbu_internal_flexspi_nor_register_handler("QSPI", "/dev/m25p0.barebox", 0);

	/* Enable RGMII TX clk output */
	val = readl(MX8MP_IOMUXC_GPR_BASE_ADDR + MX8MP_IOMUXC_GPR1);
	val |= MX8MP_IOMUXC_GPR1_ENET1_RGMII_EN;
	writel(val, MX8MP_IOMUXC_GPR_BASE_ADDR + MX8MP_IOMUXC_GPR1);

	return 0;
}

static const struct of_device_id nxp_imx8mp_evk_of_match[] = {
	{ .compatible = "fsl,imx8mp-evk" },
	{ /* Sentinel */ }
};
BAREBOX_DEEP_PROBE_ENABLE(nxp_imx8mp_evk_of_match);

static struct driver nxp_imx8mp_evk_board_driver = {
	.name = "board-nxp-imx8mp-evk",
	.probe = nxp_imx8mp_evk_probe,
	.of_compatible = nxp_imx8mp_evk_of_match,
};
coredevice_platform_driver(nxp_imx8mp_evk_board_driver);
