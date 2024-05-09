// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2018 Sascha Hauer, Pengutronix

#include <asm/memory.h>
#include <bootsource.h>
#include <common.h>
#include <init.h>
#include <linux/phy.h>
#include <linux/sizes.h>
#include <mach/imx/bbu.h>
#include <deep-probe.h>

#include <envfs.h>

#define PHY_ID_AR8031	0x004dd074
#define AR_PHY_ID_MASK	0xffffffff

static int ar8031_phy_fixup(struct phy_device *phydev)
{
	/*
	 * Enable 1.8V(SEL_1P5_1P8_POS_REG) on
	 * Phy control debug reg 0
	 */
	phy_write(phydev, 0x1d, 0x1f);
	phy_write(phydev, 0x1e, 0x8);

	/* rgmii tx clock delay enable */
	phy_write(phydev, 0x1d, 0x05);
	phy_write(phydev, 0x1e, 0x100);

	return 0;
}

static int imx8mm_evk_probe(struct device *dev)
{
	int emmc_bbu_flag = 0;
	int sd_bbu_flag = 0;

	barebox_set_hostname("imx8mm-evk");

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

	phy_register_fixup_for_uid(PHY_ID_AR8031, AR_PHY_ID_MASK,
				   ar8031_phy_fixup);
	return 0;
}

static const struct of_device_id imx8mm_evk_of_match[] = {
	{ .compatible = "fsl,imx8mm-evk", },
	{ .compatible = "fsl,imx8mm-evkb", },
	{ /* sentinel */ }
};

static struct driver imx8mm_evk_board_driver = {
	.name = "board-imx8mm-evk",
	.probe = imx8mm_evk_probe,
	.of_compatible = imx8mm_evk_of_match,
};
coredevice_platform_driver(imx8mm_evk_board_driver);

BAREBOX_DEEP_PROBE_ENABLE(imx8mm_evk_of_match);
