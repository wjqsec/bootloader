// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright 2017 Impinj, Inc
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * Based on the code of analogus driver:
 *
 * Copyright 2015-2017 Pengutronix, Lucas Stach <kernel@pengutronix.de>
 */

#include <of_device.h>
#include <common.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <clock.h>
#include <abort.h>
#include <malloc.h>
#include <io.h>
#include <init.h>
#include <linux/iopoll.h>
#include <linux/sizes.h>

#include <pm_domain.h>
#include <regulator.h>
#include <dt-bindings/power/imx7-power.h>
#include <dt-bindings/power/imx8mq-power.h>
#include <dt-bindings/power/imx8mm-power.h>
#include <dt-bindings/power/imx8mn-power.h>
#include <dt-bindings/power/imx8mp-power.h>

#define GPC_LPCR_A_CORE_BSC		0x000

#define GPC_PGC_CPU_MAPPING		0x0ec
#define IMX8MP_GPC_PGC_CPU_MAPPING	0x1cc

#define IMX7_USB_HSIC_PHY_A_CORE_DOMAIN		BIT(6)
#define IMX7_USB_OTG2_PHY_A_CORE_DOMAIN		BIT(5)
#define IMX7_USB_OTG1_PHY_A_CORE_DOMAIN		BIT(4)
#define IMX7_PCIE_PHY_A_CORE_DOMAIN		BIT(3)
#define IMX7_MIPI_PHY_A_CORE_DOMAIN		BIT(2)

#define IMX8M_PCIE2_A53_DOMAIN		BIT(15)
#define IMX8M_MIPI_CSI2_A53_DOMAIN	BIT(14)
#define IMX8M_MIPI_CSI1_A53_DOMAIN	BIT(13)
#define IMX8M_DISP_A53_DOMAIN		BIT(12)
#define IMX8M_HDMI_A53_DOMAIN		BIT(11)
#define IMX8M_VPU_A53_DOMAIN		BIT(10)
#define IMX8M_GPU_A53_DOMAIN		BIT(9)
#define IMX8M_DDR2_A53_DOMAIN		BIT(8)
#define IMX8M_DDR1_A53_DOMAIN		BIT(7)
#define IMX8M_OTG2_A53_DOMAIN		BIT(5)
#define IMX8M_OTG1_A53_DOMAIN		BIT(4)
#define IMX8M_PCIE1_A53_DOMAIN		BIT(3)
#define IMX8M_MIPI_A53_DOMAIN		BIT(2)

#define IMX8MM_VPUH1_A53_DOMAIN		BIT(15)
#define IMX8MM_VPUG2_A53_DOMAIN		BIT(14)
#define IMX8MM_VPUG1_A53_DOMAIN		BIT(13)
#define IMX8MM_DISPMIX_A53_DOMAIN	BIT(12)
#define IMX8MM_VPUMIX_A53_DOMAIN	BIT(10)
#define IMX8MM_GPUMIX_A53_DOMAIN	BIT(9)
#define IMX8MM_GPU_A53_DOMAIN		(BIT(8) | BIT(11))
#define IMX8MM_DDR1_A53_DOMAIN		BIT(7)
#define IMX8MM_OTG2_A53_DOMAIN		BIT(5)
#define IMX8MM_OTG1_A53_DOMAIN		BIT(4)
#define IMX8MM_PCIE_A53_DOMAIN		BIT(3)
#define IMX8MM_MIPI_A53_DOMAIN		BIT(2)

#define IMX8MN_DISPMIX_A53_DOMAIN	BIT(12)
#define IMX8MN_GPUMIX_A53_DOMAIN	BIT(9)
#define IMX8MN_DDR1_A53_DOMAIN		BIT(7)
#define IMX8MN_OTG1_A53_DOMAIN		BIT(4)
#define IMX8MN_MIPI_A53_DOMAIN		BIT(2)

#define IMX8MP_MEDIA_ISPDWP_A53_DOMAIN	BIT(20)
#define IMX8MP_HSIOMIX_A53_DOMAIN		BIT(19)
#define IMX8MP_MIPI_PHY2_A53_DOMAIN		BIT(18)
#define IMX8MP_HDMI_PHY_A53_DOMAIN		BIT(17)
#define IMX8MP_HDMIMIX_A53_DOMAIN		BIT(16)
#define IMX8MP_VPU_VC8000E_A53_DOMAIN		BIT(15)
#define IMX8MP_VPU_G2_A53_DOMAIN		BIT(14)
#define IMX8MP_VPU_G1_A53_DOMAIN		BIT(13)
#define IMX8MP_MEDIAMIX_A53_DOMAIN		BIT(12)
#define IMX8MP_GPU3D_A53_DOMAIN			BIT(11)
#define IMX8MP_VPUMIX_A53_DOMAIN		BIT(10)
#define IMX8MP_GPUMIX_A53_DOMAIN		BIT(9)
#define IMX8MP_GPU2D_A53_DOMAIN			BIT(8)
#define IMX8MP_AUDIOMIX_A53_DOMAIN		BIT(7)
#define IMX8MP_MLMIX_A53_DOMAIN			BIT(6)
#define IMX8MP_USB2_PHY_A53_DOMAIN		BIT(5)
#define IMX8MP_USB1_PHY_A53_DOMAIN		BIT(4)
#define IMX8MP_PCIE_PHY_A53_DOMAIN		BIT(3)
#define IMX8MP_MIPI_PHY1_A53_DOMAIN		BIT(2)

#define IMX8MP_GPC_PU_PGC_SW_PUP_REQ	0x0d8
#define IMX8MP_GPC_PU_PGC_SW_PDN_REQ	0x0e4

#define GPC_PU_PGC_SW_PUP_REQ		0x0f8
#define GPC_PU_PGC_SW_PDN_REQ		0x104

#define IMX7_USB_HSIC_PHY_SW_Pxx_REQ	BIT(4)
#define IMX7_USB_OTG2_PHY_SW_Pxx_REQ	BIT(3)
#define IMX7_USB_OTG1_PHY_SW_Pxx_REQ	BIT(2)
#define IMX7_PCIE_PHY_SW_Pxx_REQ	BIT(1)
#define IMX7_MIPI_PHY_SW_Pxx_REQ	BIT(0)

#define IMX8M_PCIE2_SW_Pxx_REQ		BIT(13)
#define IMX8M_MIPI_CSI2_SW_Pxx_REQ	BIT(12)
#define IMX8M_MIPI_CSI1_SW_Pxx_REQ	BIT(11)
#define IMX8M_DISP_SW_Pxx_REQ		BIT(10)
#define IMX8M_HDMI_SW_Pxx_REQ		BIT(9)
#define IMX8M_VPU_SW_Pxx_REQ		BIT(8)
#define IMX8M_GPU_SW_Pxx_REQ		BIT(7)
#define IMX8M_DDR2_SW_Pxx_REQ		BIT(6)
#define IMX8M_DDR1_SW_Pxx_REQ		BIT(5)
#define IMX8M_OTG2_SW_Pxx_REQ		BIT(3)
#define IMX8M_OTG1_SW_Pxx_REQ		BIT(2)
#define IMX8M_PCIE1_SW_Pxx_REQ		BIT(1)
#define IMX8M_MIPI_SW_Pxx_REQ		BIT(0)

#define IMX8MM_VPUH1_SW_Pxx_REQ		BIT(13)
#define IMX8MM_VPUG2_SW_Pxx_REQ		BIT(12)
#define IMX8MM_VPUG1_SW_Pxx_REQ		BIT(11)
#define IMX8MM_DISPMIX_SW_Pxx_REQ	BIT(10)
#define IMX8MM_VPUMIX_SW_Pxx_REQ	BIT(8)
#define IMX8MM_GPUMIX_SW_Pxx_REQ	BIT(7)
#define IMX8MM_GPU_SW_Pxx_REQ		(BIT(6) | BIT(9))
#define IMX8MM_DDR1_SW_Pxx_REQ		BIT(5)
#define IMX8MM_OTG2_SW_Pxx_REQ		BIT(3)
#define IMX8MM_OTG1_SW_Pxx_REQ		BIT(2)
#define IMX8MM_PCIE_SW_Pxx_REQ		BIT(1)
#define IMX8MM_MIPI_SW_Pxx_REQ		BIT(0)

#define IMX8MN_DISPMIX_SW_Pxx_REQ		BIT(10)
#define IMX8MN_GPUMIX_SW_Pxx_REQ		BIT(7)
#define IMX8MN_DDR1_SW_Pxx_REQ		BIT(5)
#define IMX8MN_OTG1_SW_Pxx_REQ		BIT(2)
#define IMX8MN_MIPI_SW_Pxx_REQ		BIT(0)

#define IMX8MP_DDRMIX_Pxx_REQ			BIT(19)
#define IMX8MP_MEDIA_ISP_DWP_Pxx_REQ		BIT(18)
#define IMX8MP_HSIOMIX_Pxx_REQ			BIT(17)
#define IMX8MP_MIPI_PHY2_Pxx_REQ		BIT(16)
#define IMX8MP_HDMI_PHY_Pxx_REQ			BIT(15)
#define IMX8MP_HDMIMIX_Pxx_REQ			BIT(14)
#define IMX8MP_VPU_VC8K_Pxx_REQ			BIT(13)
#define IMX8MP_VPU_G2_Pxx_REQ			BIT(12)
#define IMX8MP_VPU_G1_Pxx_REQ			BIT(11)
#define IMX8MP_MEDIMIX_Pxx_REQ			BIT(10)
#define IMX8MP_GPU_3D_Pxx_REQ			BIT(9)
#define IMX8MP_VPU_MIX_SHARE_LOGIC_Pxx_REQ	BIT(8)
#define IMX8MP_GPU_SHARE_LOGIC_Pxx_REQ		BIT(7)
#define IMX8MP_GPU_2D_Pxx_REQ			BIT(6)
#define IMX8MP_AUDIOMIX_Pxx_REQ			BIT(5)
#define IMX8MP_MLMIX_Pxx_REQ			BIT(4)
#define IMX8MP_USB2_PHY_Pxx_REQ			BIT(3)
#define IMX8MP_USB1_PHY_Pxx_REQ			BIT(2)
#define IMX8MP_PCIE_PHY_SW_Pxx_REQ		BIT(1)
#define IMX8MP_MIPI_PHY1_SW_Pxx_REQ		BIT(0)

#define GPC_M4_PU_PDN_FLG		0x1bc

#define IMX8MP_GPC_PU_PWRHSK		0x190
#define GPC_PU_PWRHSK			0x1fc

#define IMX8M_GPU_HSK_PWRDNACKN			BIT(26)
#define IMX8M_VPU_HSK_PWRDNACKN			BIT(25)
#define IMX8M_DISP_HSK_PWRDNACKN		BIT(24)
#define IMX8M_GPU_HSK_PWRDNREQN			BIT(6)
#define IMX8M_VPU_HSK_PWRDNREQN			BIT(5)
#define IMX8M_DISP_HSK_PWRDNREQN		BIT(4)


#define IMX8MM_GPUMIX_HSK_PWRDNACKN		BIT(29)
#define IMX8MM_GPU_HSK_PWRDNACKN		(BIT(27) | BIT(28))
#define IMX8MM_VPUMIX_HSK_PWRDNACKN		BIT(26)
#define IMX8MM_DISPMIX_HSK_PWRDNACKN		BIT(25)
#define IMX8MM_HSIO_HSK_PWRDNACKN		(BIT(23) | BIT(24))
#define IMX8MM_GPUMIX_HSK_PWRDNREQN		BIT(11)
#define IMX8MM_GPU_HSK_PWRDNREQN		(BIT(9) | BIT(10))
#define IMX8MM_VPUMIX_HSK_PWRDNREQN		BIT(8)
#define IMX8MM_DISPMIX_HSK_PWRDNREQN		BIT(7)
#define IMX8MM_HSIO_HSK_PWRDNREQN		(BIT(5) | BIT(6))

#define IMX8MN_GPUMIX_HSK_PWRDNACKN		(BIT(29) | BIT(27))
#define IMX8MN_DISPMIX_HSK_PWRDNACKN		BIT(25)
#define IMX8MN_HSIO_HSK_PWRDNACKN		BIT(23)
#define IMX8MN_GPUMIX_HSK_PWRDNREQN		(BIT(11) | BIT(9))
#define IMX8MN_DISPMIX_HSK_PWRDNREQN		BIT(7)
#define IMX8MN_HSIO_HSK_PWRDNREQN		BIT(5)

#define IMX8MP_MEDIAMIX_PWRDNACKN		BIT(30)
#define IMX8MP_HDMIMIX_PWRDNACKN		BIT(29)
#define IMX8MP_HSIOMIX_PWRDNACKN		BIT(28)
#define IMX8MP_VPUMIX_PWRDNACKN			BIT(26)
#define IMX8MP_GPUMIX_PWRDNACKN			BIT(25)
#define IMX8MP_MLMIX_PWRDNACKN			(BIT(23) | BIT(24))
#define IMX8MP_AUDIOMIX_PWRDNACKN		(BIT(20) | BIT(31))
#define IMX8MP_MEDIAMIX_PWRDNREQN		BIT(14)
#define IMX8MP_HDMIMIX_PWRDNREQN		BIT(13)
#define IMX8MP_HSIOMIX_PWRDNREQN		BIT(12)
#define IMX8MP_VPUMIX_PWRDNREQN			BIT(10)
#define IMX8MP_GPUMIX_PWRDNREQN			BIT(9)
#define IMX8MP_MLMIX_PWRDNREQN			(BIT(7) | BIT(8))
#define IMX8MP_AUDIOMIX_PWRDNREQN		(BIT(4) | BIT(15))

/*
 * The PGC offset values in Reference Manual
 * (Rev. 1, 01/2018 and the older ones) GPC chapter's
 * GPC_PGC memory map are incorrect, below offset
 * values are from design RTL.
 */
#define IMX7_PGC_MIPI                       16
#define IMX7_PGC_PCIE                       17
#define IMX7_PGC_USB_HSIC                   20


#define IMX8M_PGC_MIPI			16
#define IMX8M_PGC_PCIE1		17
#define IMX8M_PGC_OTG1			18
#define IMX8M_PGC_OTG2			19
#define IMX8M_PGC_DDR1			21
#define IMX8M_PGC_GPU			23
#define IMX8M_PGC_VPU			24
#define IMX8M_PGC_DISP			26
#define IMX8M_PGC_MIPI_CSI1		27
#define IMX8M_PGC_MIPI_CSI2		28
#define IMX8M_PGC_PCIE2		29

#define IMX8MM_PGC_MIPI			16
#define IMX8MM_PGC_PCIE			17
#define IMX8MM_PGC_OTG1			18
#define IMX8MM_PGC_OTG2			19
#define IMX8MM_PGC_DDR1			21
#define IMX8MM_PGC_GPU2D		22
#define IMX8MM_PGC_GPUMIX		23
#define IMX8MM_PGC_VPUMIX		24
#define IMX8MM_PGC_GPU3D		25
#define IMX8MM_PGC_DISPMIX		26
#define IMX8MM_PGC_VPUG1		27
#define IMX8MM_PGC_VPUG2		28
#define IMX8MM_PGC_VPUH1		29

#define IMX8MN_PGC_MIPI		16
#define IMX8MN_PGC_OTG1		18
#define IMX8MN_PGC_DDR1		21
#define IMX8MN_PGC_GPUMIX		23
#define IMX8MN_PGC_DISPMIX		26

#define IMX8MP_PGC_NOC			9
#define IMX8MP_PGC_MIPI1		12
#define IMX8MP_PGC_PCIE			13
#define IMX8MP_PGC_USB1			14
#define IMX8MP_PGC_USB2			15
#define IMX8MP_PGC_MLMIX		16
#define IMX8MP_PGC_AUDIOMIX		17
#define IMX8MP_PGC_GPU2D		18
#define IMX8MP_PGC_GPUMIX		19
#define IMX8MP_PGC_VPUMIX		20
#define IMX8MP_PGC_GPU3D		21
#define IMX8MP_PGC_MEDIAMIX		22
#define IMX8MP_PGC_VPU_G1		23
#define IMX8MP_PGC_VPU_G2		24
#define IMX8MP_PGC_VPU_VC8000E		25
#define IMX8MP_PGC_HDMIMIX		26
#define IMX8MP_PGC_HDMI			27
#define IMX8MP_PGC_MIPI2		28
#define IMX8MP_PGC_HSIOMIX		29
#define IMX8MP_PGC_MEDIA_ISP_DWP	30
#define IMX8MP_PGC_DDRMIX		31

#define GPC_PGC_CTRL(n)			(0x800 + (n) * 0x40)
#define GPC_PGC_SR(n)			(GPC_PGC_CTRL(n) + 0xc)

#define GPC_PGC_CTRL_PCR		BIT(0)

struct imx_pgc_regs {
	u16 map;
	u16 pup;
	u16 pdn;
	u16 hsk;
};

struct imx_pgc_domain {
	struct generic_pm_domain genpd;
	struct regmap *regmap;
	const struct imx_pgc_regs *regs;
	struct regulator *regulator;
	struct reset_control *reset;
	struct clk_bulk_data *clks;
	int num_clks;

	unsigned long pgc;

	const struct {
		u32 pxx;
		u32 map;
		u32 hskreq;
		u32 hskack;
	} bits;

	const int voltage;
	const bool keep_clocks;
	struct device *dev;

	unsigned int pgc_sw_pup_reg;
	unsigned int pgc_sw_pdn_reg;
};

struct imx_pgc_domain_data {
	const struct imx_pgc_domain *domains;
	size_t domains_num;
	const struct imx_pgc_regs *pgc_regs;
};

static inline struct imx_pgc_domain *
to_imx_pgc_domain(struct generic_pm_domain *genpd)
{
	return container_of(genpd, struct imx_pgc_domain, genpd);
}

static int imx_pgc_power_up(struct generic_pm_domain *genpd)
{
	struct imx_pgc_domain *domain = to_imx_pgc_domain(genpd);
	u32 reg_val, pgc;
	int ret;

	if (!IS_ERR(domain->regulator)) {
		ret = regulator_enable(domain->regulator);
		if (ret) {
			dev_err(domain->dev, "failed to enable regulator\n");
			return ret;
		}
	}

	/* Enable reset clocks for all devices in the domain */
	ret = clk_bulk_enable(domain->num_clks, domain->clks);
	if (ret) {
		dev_err(domain->dev, "failed to enable reset clocks\n");
		goto out_regulator_disable;
	}

	reset_control_assert(domain->reset);

	if (domain->bits.pxx) {
		/* request the domain to power up */
		regmap_update_bits(domain->regmap, domain->regs->pup,
				   domain->bits.pxx, domain->bits.pxx);
		/*
		 * As per "5.5.9.4 Example Code 4" in IMX7DRM.pdf wait
		 * for PUP_REQ/PDN_REQ bit to be cleared
		 */
		ret = regmap_read_poll_timeout(domain->regmap,
					       domain->regs->pup, reg_val,
					       !(reg_val & domain->bits.pxx),
					       USEC_PER_MSEC);
		if (ret) {
			dev_err(domain->dev, "failed to command PGC\n");
			goto out_clk_disable;
		}

		/* disable power control */
		for_each_set_bit(pgc, &domain->pgc, 32) {
			regmap_clear_bits(domain->regmap, GPC_PGC_CTRL(pgc),
					  GPC_PGC_CTRL_PCR);
		}
	}

	/* delay for reset to propagate */
	udelay(5);

	reset_control_deassert(domain->reset);

	/* request the ADB400 to power up */
	if (domain->bits.hskreq) {
		regmap_update_bits(domain->regmap, domain->regs->hsk,
				   domain->bits.hskreq, domain->bits.hskreq);

		/*
		 * ret = regmap_read_poll_timeout(domain->regmap, domain->regs->hsk, reg_val,
		 *				  (reg_val & domain->bits.hskack),
		 *				  USEC_PER_MSEC);
		 * Technically we need the commented code to wait handshake. But that needs
		 * the BLK-CTL module BUS clk-en bit being set.
		 *
		 * There is a separate BLK-CTL module and we will have such a driver for it,
		 * that driver will set the BUS clk-en bit and handshake will be triggered
		 * automatically there. Just add a delay and suppose the handshake finish
		 * after that.
		 */
	}

	/* Disable reset clocks for all devices in the domain */
	if (!domain->keep_clocks)
		clk_bulk_disable(domain->num_clks, domain->clks);

	return 0;

out_clk_disable:
	clk_bulk_disable(domain->num_clks, domain->clks);
out_regulator_disable:
	if (!IS_ERR(domain->regulator))
		regulator_disable(domain->regulator);

	return ret;
}

static int imx_pgc_power_down(struct generic_pm_domain *genpd)
{
	struct imx_pgc_domain *domain = to_imx_pgc_domain(genpd);
	u32 reg_val, pgc;
	int ret;

	/* Enable reset clocks for all devices in the domain */
	if (!domain->keep_clocks) {
		ret = clk_bulk_enable(domain->num_clks, domain->clks);
		if (ret) {
			dev_err(domain->dev, "failed to enable reset clocks\n");
			return ret;
		}
	}

	/* request the ADB400 to power down */
	if (domain->bits.hskreq) {
		regmap_clear_bits(domain->regmap, domain->regs->hsk,
				  domain->bits.hskreq);

		ret = regmap_read_poll_timeout(domain->regmap, domain->regs->hsk,
					       reg_val,
					       !(reg_val & domain->bits.hskack),
					       USEC_PER_MSEC);
		if (ret) {
			dev_err(domain->dev, "failed to power down ADB400\n");
			goto out_clk_disable;
		}
	}

	if (domain->bits.pxx) {
		/* enable power control */
		for_each_set_bit(pgc, &domain->pgc, 32) {
			regmap_update_bits(domain->regmap, GPC_PGC_CTRL(pgc),
					   GPC_PGC_CTRL_PCR, GPC_PGC_CTRL_PCR);
		}

		/* request the domain to power down */
		regmap_update_bits(domain->regmap, domain->regs->pdn,
				   domain->bits.pxx, domain->bits.pxx);
		/*
		 * As per "5.5.9.4 Example Code 4" in IMX7DRM.pdf wait
		 * for PUP_REQ/PDN_REQ bit to be cleared
		 */
		ret = regmap_read_poll_timeout(domain->regmap,
					       domain->regs->pdn, reg_val,
					       !(reg_val & domain->bits.pxx),
					       USEC_PER_MSEC);
		if (ret) {
			dev_err(domain->dev, "failed to command PGC\n");
			goto out_clk_disable;
		}
	}

	/* Disable reset clocks for all devices in the domain */
	clk_bulk_disable(domain->num_clks, domain->clks);

	if (!IS_ERR(domain->regulator)) {
		ret = regulator_disable(domain->regulator);
		if (ret) {
			dev_err(domain->dev, "failed to disable regulator\n");
			return ret;
		}
	}

	return 0;

out_clk_disable:
	if (!domain->keep_clocks)
		clk_bulk_disable(domain->num_clks, domain->clks);

	return ret;
}

static const struct imx_pgc_domain imx7_pgc_domains[] = {
	[IMX7_POWER_DOMAIN_MIPI_PHY] = {
		.genpd = {
			.name      = "mipi-phy",
		},
		.bits  = {
			.pxx = IMX7_MIPI_PHY_SW_Pxx_REQ,
			.map = IMX7_MIPI_PHY_A_CORE_DOMAIN,
		},
		.voltage   = 1000000,
		.pgc	   = BIT(IMX7_PGC_MIPI),
	},

	[IMX7_POWER_DOMAIN_PCIE_PHY] = {
		.genpd = {
			.name      = "pcie-phy",
		},
		.bits  = {
			.pxx = IMX7_PCIE_PHY_SW_Pxx_REQ,
			.map = IMX7_PCIE_PHY_A_CORE_DOMAIN,
		},
		.voltage   = 1000000,
		.pgc	   = BIT(IMX7_PGC_PCIE),
	},

	[IMX7_POWER_DOMAIN_USB_HSIC_PHY] = {
		.genpd = {
			.name      = "usb-hsic-phy",
		},
		.bits  = {
			.pxx = IMX7_USB_HSIC_PHY_SW_Pxx_REQ,
			.map = IMX7_USB_HSIC_PHY_A_CORE_DOMAIN,
		},
		.voltage   = 1200000,
		.pgc	   = BIT(IMX7_PGC_USB_HSIC),
	},
};

static const struct imx_pgc_regs imx7_pgc_regs = {
	.map = GPC_PGC_CPU_MAPPING,
	.pup = GPC_PU_PGC_SW_PUP_REQ,
	.pdn = GPC_PU_PGC_SW_PDN_REQ,
	.hsk = GPC_PU_PWRHSK,
};

static const struct imx_pgc_domain_data imx7_pgc_domain_data = {
	.domains = imx7_pgc_domains,
	.domains_num = ARRAY_SIZE(imx7_pgc_domains),
	.pgc_regs = &imx7_pgc_regs,
};

static const struct imx_pgc_domain imx8m_pgc_domains[] = {
	[IMX8M_POWER_DOMAIN_MIPI] = {
		.genpd = {
			.name      = "mipi",
		},
		.bits  = {
			.pxx = IMX8M_MIPI_SW_Pxx_REQ,
			.map = IMX8M_MIPI_A53_DOMAIN,
		},
		.pgc	   = BIT(IMX8M_PGC_MIPI),
	},

	[IMX8M_POWER_DOMAIN_PCIE1] = {
		.genpd = {
			.name = "pcie1",
		},
		.bits  = {
			.pxx = IMX8M_PCIE1_SW_Pxx_REQ,
			.map = IMX8M_PCIE1_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8M_PGC_PCIE1),
	},

	[IMX8M_POWER_DOMAIN_USB_OTG1] = {
		.genpd = {
			.name = "usb-otg1",
		},
		.bits  = {
			.pxx = IMX8M_OTG1_SW_Pxx_REQ,
			.map = IMX8M_OTG1_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8M_PGC_OTG1),
	},

	[IMX8M_POWER_DOMAIN_USB_OTG2] = {
		.genpd = {
			.name = "usb-otg2",
		},
		.bits  = {
			.pxx = IMX8M_OTG2_SW_Pxx_REQ,
			.map = IMX8M_OTG2_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8M_PGC_OTG2),
	},

	[IMX8M_POWER_DOMAIN_DDR1] = {
		.genpd = {
			.name = "ddr1",
		},
		.bits  = {
			.pxx = IMX8M_DDR1_SW_Pxx_REQ,
			.map = IMX8M_DDR2_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8M_PGC_DDR1),
	},

	[IMX8M_POWER_DOMAIN_GPU] = {
		.genpd = {
			.name = "gpu",
		},
		.bits  = {
			.pxx = IMX8M_GPU_SW_Pxx_REQ,
			.map = IMX8M_GPU_A53_DOMAIN,
			.hskreq = IMX8M_GPU_HSK_PWRDNREQN,
			.hskack = IMX8M_GPU_HSK_PWRDNACKN,
		},
		.pgc   = BIT(IMX8M_PGC_GPU),
	},

	[IMX8M_POWER_DOMAIN_VPU] = {
		.genpd = {
			.name = "vpu",
		},
		.bits  = {
			.pxx = IMX8M_VPU_SW_Pxx_REQ,
			.map = IMX8M_VPU_A53_DOMAIN,
			.hskreq = IMX8M_VPU_HSK_PWRDNREQN,
			.hskack = IMX8M_VPU_HSK_PWRDNACKN,
		},
		.pgc   = BIT(IMX8M_PGC_VPU),
		.keep_clocks = true,
	},

	[IMX8M_POWER_DOMAIN_DISP] = {
		.genpd = {
			.name = "disp",
		},
		.bits  = {
			.pxx = IMX8M_DISP_SW_Pxx_REQ,
			.map = IMX8M_DISP_A53_DOMAIN,
			.hskreq = IMX8M_DISP_HSK_PWRDNREQN,
			.hskack = IMX8M_DISP_HSK_PWRDNACKN,
		},
		.pgc   = BIT(IMX8M_PGC_DISP),
	},

	[IMX8M_POWER_DOMAIN_MIPI_CSI1] = {
		.genpd = {
			.name = "mipi-csi1",
		},
		.bits  = {
			.pxx = IMX8M_MIPI_CSI1_SW_Pxx_REQ,
			.map = IMX8M_MIPI_CSI1_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8M_PGC_MIPI_CSI1),
	},

	[IMX8M_POWER_DOMAIN_MIPI_CSI2] = {
		.genpd = {
			.name = "mipi-csi2",
		},
		.bits  = {
			.pxx = IMX8M_MIPI_CSI2_SW_Pxx_REQ,
			.map = IMX8M_MIPI_CSI2_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8M_PGC_MIPI_CSI2),
	},

	[IMX8M_POWER_DOMAIN_PCIE2] = {
		.genpd = {
			.name = "pcie2",
		},
		.bits  = {
			.pxx = IMX8M_PCIE2_SW_Pxx_REQ,
			.map = IMX8M_PCIE2_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8M_PGC_PCIE2),
	},
};

static const struct imx_pgc_domain_data imx8m_pgc_domain_data = {
	.domains = imx8m_pgc_domains,
	.domains_num = ARRAY_SIZE(imx8m_pgc_domains),
	.pgc_regs = &imx7_pgc_regs,
};

static const struct imx_pgc_domain imx8mm_pgc_domains[] = {
	[IMX8MM_POWER_DOMAIN_HSIOMIX] = {
		.genpd = {
			.name = "hsiomix",
		},
		.bits  = {
			.pxx = 0, /* no power sequence control */
			.map = 0, /* no power sequence control */
			.hskreq = IMX8MM_HSIO_HSK_PWRDNREQN,
			.hskack = IMX8MM_HSIO_HSK_PWRDNACKN,
		},
		.keep_clocks = true,
	},

	[IMX8MM_POWER_DOMAIN_PCIE] = {
		.genpd = {
			.name = "pcie",
		},
		.bits  = {
			.pxx = IMX8MM_PCIE_SW_Pxx_REQ,
			.map = IMX8MM_PCIE_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8MM_PGC_PCIE),
	},

	[IMX8MM_POWER_DOMAIN_OTG1] = {
		.genpd = {
			.name = "usb-otg1",
		},
		.bits  = {
			.pxx = IMX8MM_OTG1_SW_Pxx_REQ,
			.map = IMX8MM_OTG1_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8MM_PGC_OTG1),
	},

	[IMX8MM_POWER_DOMAIN_OTG2] = {
		.genpd = {
			.name = "usb-otg2",
		},
		.bits  = {
			.pxx = IMX8MM_OTG2_SW_Pxx_REQ,
			.map = IMX8MM_OTG2_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8MM_PGC_OTG2),
	},

	[IMX8MM_POWER_DOMAIN_GPUMIX] = {
		.genpd = {
			.name = "gpumix",
		},
		.bits  = {
			.pxx = IMX8MM_GPUMIX_SW_Pxx_REQ,
			.map = IMX8MM_GPUMIX_A53_DOMAIN,
			.hskreq = IMX8MM_GPUMIX_HSK_PWRDNREQN,
			.hskack = IMX8MM_GPUMIX_HSK_PWRDNACKN,
		},
		.pgc   = BIT(IMX8MM_PGC_GPUMIX),
		.keep_clocks = true,
	},

	[IMX8MM_POWER_DOMAIN_GPU] = {
		.genpd = {
			.name = "gpu",
		},
		.bits  = {
			.pxx = IMX8MM_GPU_SW_Pxx_REQ,
			.map = IMX8MM_GPU_A53_DOMAIN,
			.hskreq = IMX8MM_GPU_HSK_PWRDNREQN,
			.hskack = IMX8MM_GPU_HSK_PWRDNACKN,
		},
		.pgc   = BIT(IMX8MM_PGC_GPU2D) | BIT(IMX8MM_PGC_GPU3D),
	},

	[IMX8MM_POWER_DOMAIN_VPUMIX] = {
		.genpd = {
			.name = "vpumix",
		},
		.bits  = {
			.pxx = IMX8MM_VPUMIX_SW_Pxx_REQ,
			.map = IMX8MM_VPUMIX_A53_DOMAIN,
			.hskreq = IMX8MM_VPUMIX_HSK_PWRDNREQN,
			.hskack = IMX8MM_VPUMIX_HSK_PWRDNACKN,
		},
		.pgc   = BIT(IMX8MM_PGC_VPUMIX),
		.keep_clocks = true,
	},

	[IMX8MM_POWER_DOMAIN_VPUG1] = {
		.genpd = {
			.name = "vpu-g1",
		},
		.bits  = {
			.pxx = IMX8MM_VPUG1_SW_Pxx_REQ,
			.map = IMX8MM_VPUG1_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8MM_PGC_VPUG1),
	},

	[IMX8MM_POWER_DOMAIN_VPUG2] = {
		.genpd = {
			.name = "vpu-g2",
		},
		.bits  = {
			.pxx = IMX8MM_VPUG2_SW_Pxx_REQ,
			.map = IMX8MM_VPUG2_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8MM_PGC_VPUG2),
	},

	[IMX8MM_POWER_DOMAIN_VPUH1] = {
		.genpd = {
			.name = "vpu-h1",
		},
		.bits  = {
			.pxx = IMX8MM_VPUH1_SW_Pxx_REQ,
			.map = IMX8MM_VPUH1_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8MM_PGC_VPUH1),
		.keep_clocks = true,
	},

	[IMX8MM_POWER_DOMAIN_DISPMIX] = {
		.genpd = {
			.name = "dispmix",
		},
		.bits  = {
			.pxx = IMX8MM_DISPMIX_SW_Pxx_REQ,
			.map = IMX8MM_DISPMIX_A53_DOMAIN,
			.hskreq = IMX8MM_DISPMIX_HSK_PWRDNREQN,
			.hskack = IMX8MM_DISPMIX_HSK_PWRDNACKN,
		},
		.pgc   = BIT(IMX8MM_PGC_DISPMIX),
		.keep_clocks = true,
	},

	[IMX8MM_POWER_DOMAIN_MIPI] = {
		.genpd = {
			.name = "mipi",
		},
		.bits  = {
			.pxx = IMX8MM_MIPI_SW_Pxx_REQ,
			.map = IMX8MM_MIPI_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8MM_PGC_MIPI),
	},
};

static const struct imx_pgc_domain_data imx8mm_pgc_domain_data = {
	.domains = imx8mm_pgc_domains,
	.domains_num = ARRAY_SIZE(imx8mm_pgc_domains),
	.pgc_regs = &imx7_pgc_regs,
};

static const struct imx_pgc_domain imx8mp_pgc_domains[] = {
	[IMX8MP_POWER_DOMAIN_MIPI_PHY1] = {
		.genpd = {
			.name = "mipi-phy1",
		},
		.bits = {
			.pxx = IMX8MP_MIPI_PHY1_SW_Pxx_REQ,
			.map = IMX8MP_MIPI_PHY1_A53_DOMAIN,
		},
		.pgc = BIT(IMX8MP_PGC_MIPI1),
	},

	[IMX8MP_POWER_DOMAIN_PCIE_PHY] = {
		.genpd = {
			.name = "pcie-phy1",
		},
		.bits = {
			.pxx = IMX8MP_PCIE_PHY_SW_Pxx_REQ,
			.map = IMX8MP_PCIE_PHY_A53_DOMAIN,
		},
		.pgc = BIT(IMX8MP_PGC_PCIE),
	},

	[IMX8MP_POWER_DOMAIN_USB1_PHY] = {
		.genpd = {
			.name = "usb-otg1",
		},
		.bits = {
			.pxx = IMX8MP_USB1_PHY_Pxx_REQ,
			.map = IMX8MP_USB1_PHY_A53_DOMAIN,
		},
		.pgc = BIT(IMX8MP_PGC_USB1),
	},

	[IMX8MP_POWER_DOMAIN_USB2_PHY] = {
		.genpd = {
			.name = "usb-otg2",
		},
		.bits = {
			.pxx = IMX8MP_USB2_PHY_Pxx_REQ,
			.map = IMX8MP_USB2_PHY_A53_DOMAIN,
		},
		.pgc = BIT(IMX8MP_PGC_USB2),
	},

	[IMX8MP_POWER_DOMAIN_MLMIX] = {
		.genpd = {
			.name = "mlmix",
		},
		.bits = {
			.pxx = IMX8MP_MLMIX_Pxx_REQ,
			.map = IMX8MP_MLMIX_A53_DOMAIN,
			.hskreq = IMX8MP_MLMIX_PWRDNREQN,
			.hskack = IMX8MP_MLMIX_PWRDNACKN,
		},
		.pgc = BIT(IMX8MP_PGC_MLMIX),
		.keep_clocks = true,
	},

	[IMX8MP_POWER_DOMAIN_AUDIOMIX] = {
		.genpd = {
			.name = "audiomix",
		},
		.bits = {
			.pxx = IMX8MP_AUDIOMIX_Pxx_REQ,
			.map = IMX8MP_AUDIOMIX_A53_DOMAIN,
			.hskreq = IMX8MP_AUDIOMIX_PWRDNREQN,
			.hskack = IMX8MP_AUDIOMIX_PWRDNACKN,
		},
		.pgc = BIT(IMX8MP_PGC_AUDIOMIX),
		.keep_clocks = true,
	},

	[IMX8MP_POWER_DOMAIN_GPU2D] = {
		.genpd = {
			.name = "gpu2d",
		},
		.bits = {
			.pxx = IMX8MP_GPU_2D_Pxx_REQ,
			.map = IMX8MP_GPU2D_A53_DOMAIN,
		},
		.pgc = BIT(IMX8MP_PGC_GPU2D),
	},

	[IMX8MP_POWER_DOMAIN_GPUMIX] = {
		.genpd = {
			.name = "gpumix",
		},
		.bits = {
			.pxx = IMX8MP_GPU_SHARE_LOGIC_Pxx_REQ,
			.map = IMX8MP_GPUMIX_A53_DOMAIN,
			.hskreq = IMX8MP_GPUMIX_PWRDNREQN,
			.hskack = IMX8MP_GPUMIX_PWRDNACKN,
		},
		.pgc = BIT(IMX8MP_PGC_GPUMIX),
		.keep_clocks = true,
	},

	[IMX8MP_POWER_DOMAIN_VPUMIX] = {
		.genpd = {
			.name = "vpumix",
		},
		.bits = {
			.pxx = IMX8MP_VPU_MIX_SHARE_LOGIC_Pxx_REQ,
			.map = IMX8MP_VPUMIX_A53_DOMAIN,
			.hskreq = IMX8MP_VPUMIX_PWRDNREQN,
			.hskack = IMX8MP_VPUMIX_PWRDNACKN,
		},
		.pgc = BIT(IMX8MP_PGC_VPUMIX),
		.keep_clocks = true,
	},

	[IMX8MP_POWER_DOMAIN_GPU3D] = {
		.genpd = {
			.name = "gpu3d",
		},
		.bits = {
			.pxx = IMX8MP_GPU_3D_Pxx_REQ,
			.map = IMX8MP_GPU3D_A53_DOMAIN,
		},
		.pgc = BIT(IMX8MP_PGC_GPU3D),
	},

	[IMX8MP_POWER_DOMAIN_MEDIAMIX] = {
		.genpd = {
			.name = "mediamix",
		},
		.bits = {
			.pxx = IMX8MP_MEDIMIX_Pxx_REQ,
			.map = IMX8MP_MEDIAMIX_A53_DOMAIN,
			.hskreq = IMX8MP_MEDIAMIX_PWRDNREQN,
			.hskack = IMX8MP_MEDIAMIX_PWRDNACKN,
		},
		.pgc = BIT(IMX8MP_PGC_MEDIAMIX),
		.keep_clocks = true,
	},

	[IMX8MP_POWER_DOMAIN_VPU_G1] = {
		.genpd = {
			.name = "vpu-g1",
		},
		.bits = {
			.pxx = IMX8MP_VPU_G1_Pxx_REQ,
			.map = IMX8MP_VPU_G1_A53_DOMAIN,
		},
		.pgc = BIT(IMX8MP_PGC_VPU_G1),
	},

	[IMX8MP_POWER_DOMAIN_VPU_G2] = {
		.genpd = {
			.name = "vpu-g2",
		},
		.bits = {
			.pxx = IMX8MP_VPU_G2_Pxx_REQ,
			.map = IMX8MP_VPU_G2_A53_DOMAIN
		},
		.pgc = BIT(IMX8MP_PGC_VPU_G2),
	},

	[IMX8MP_POWER_DOMAIN_VPU_VC8000E] = {
		.genpd = {
			.name = "vpu-h1",
		},
		.bits = {
			.pxx = IMX8MP_VPU_VC8K_Pxx_REQ,
			.map = IMX8MP_VPU_VC8000E_A53_DOMAIN,
		},
		.pgc = BIT(IMX8MP_PGC_VPU_VC8000E),
	},

	[IMX8MP_POWER_DOMAIN_HDMIMIX] = {
		.genpd = {
			.name = "hdmimix",
		},
		.bits = {
			.pxx = IMX8MP_HDMIMIX_Pxx_REQ,
			.map = IMX8MP_HDMIMIX_A53_DOMAIN,
			.hskreq = IMX8MP_HDMIMIX_PWRDNREQN,
			.hskack = IMX8MP_HDMIMIX_PWRDNACKN,
		},
		.pgc = BIT(IMX8MP_PGC_HDMIMIX),
		.keep_clocks = true,
	},

	[IMX8MP_POWER_DOMAIN_HDMI_PHY] = {
		.genpd = {
			.name = "hdmi-phy",
		},
		.bits = {
			.pxx = IMX8MP_HDMI_PHY_Pxx_REQ,
			.map = IMX8MP_HDMI_PHY_A53_DOMAIN,
		},
		.pgc = BIT(IMX8MP_PGC_HDMI),
	},

	[IMX8MP_POWER_DOMAIN_MIPI_PHY2] = {
		.genpd = {
			.name = "mipi-phy2",
		},
		.bits = {
			.pxx = IMX8MP_MIPI_PHY2_Pxx_REQ,
			.map = IMX8MP_MIPI_PHY2_A53_DOMAIN,
		},
		.pgc = BIT(IMX8MP_PGC_MIPI2),
	},

	[IMX8MP_POWER_DOMAIN_HSIOMIX] = {
		.genpd = {
			.name = "hsiomix",
		},
		.bits = {
			.pxx = IMX8MP_HSIOMIX_Pxx_REQ,
			.map = IMX8MP_HSIOMIX_A53_DOMAIN,
			.hskreq = IMX8MP_HSIOMIX_PWRDNREQN,
			.hskack = IMX8MP_HSIOMIX_PWRDNACKN,
		},
		.pgc = BIT(IMX8MP_PGC_HSIOMIX),
		.keep_clocks = true,
	},

	[IMX8MP_POWER_DOMAIN_MEDIAMIX_ISPDWP] = {
		.genpd = {
			.name = "mediamix-isp-dwp",
		},
		.bits = {
			.pxx = IMX8MP_MEDIA_ISP_DWP_Pxx_REQ,
			.map = IMX8MP_MEDIA_ISPDWP_A53_DOMAIN,
		},
		.pgc = BIT(IMX8MP_PGC_MEDIA_ISP_DWP),
	},
};

static const struct imx_pgc_regs imx8mp_pgc_regs = {
	.map = IMX8MP_GPC_PGC_CPU_MAPPING,
	.pup = IMX8MP_GPC_PU_PGC_SW_PUP_REQ,
	.pdn = IMX8MP_GPC_PU_PGC_SW_PDN_REQ,
	.hsk = IMX8MP_GPC_PU_PWRHSK,
};
static const struct imx_pgc_domain_data imx8mp_pgc_domain_data = {
	.domains = imx8mp_pgc_domains,
	.domains_num = ARRAY_SIZE(imx8mp_pgc_domains),
	.pgc_regs = &imx8mp_pgc_regs,
};

static const struct imx_pgc_domain imx8mn_pgc_domains[] = {
	[IMX8MN_POWER_DOMAIN_HSIOMIX] = {
		.genpd = {
			.name = "hsiomix",
		},
		.bits  = {
			.pxx = 0, /* no power sequence control */
			.map = 0, /* no power sequence control */
			.hskreq = IMX8MN_HSIO_HSK_PWRDNREQN,
			.hskack = IMX8MN_HSIO_HSK_PWRDNACKN,
		},
		.keep_clocks = true,
	},

	[IMX8MN_POWER_DOMAIN_OTG1] = {
		.genpd = {
			.name = "usb-otg1",
		},
		.bits  = {
			.pxx = IMX8MN_OTG1_SW_Pxx_REQ,
			.map = IMX8MN_OTG1_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8MN_PGC_OTG1),
	},

	[IMX8MN_POWER_DOMAIN_GPUMIX] = {
		.genpd = {
			.name = "gpumix",
		},
		.bits  = {
			.pxx = IMX8MN_GPUMIX_SW_Pxx_REQ,
			.map = IMX8MN_GPUMIX_A53_DOMAIN,
			.hskreq = IMX8MN_GPUMIX_HSK_PWRDNREQN,
			.hskack = IMX8MN_GPUMIX_HSK_PWRDNACKN,
		},
		.pgc   = BIT(IMX8MN_PGC_GPUMIX),
		.keep_clocks = true,
	},

	[IMX8MN_POWER_DOMAIN_DISPMIX] = {
		.genpd = {
			.name = "dispmix",
		},
			.bits  = {
			.pxx = IMX8MN_DISPMIX_SW_Pxx_REQ,
			.map = IMX8MN_DISPMIX_A53_DOMAIN,
			.hskreq = IMX8MN_DISPMIX_HSK_PWRDNREQN,
			.hskack = IMX8MN_DISPMIX_HSK_PWRDNACKN,
		},
		.pgc   = BIT(IMX8MN_PGC_DISPMIX),
		.keep_clocks = true,
	},

	[IMX8MN_POWER_DOMAIN_MIPI] = {
		.genpd = {
			.name = "mipi",
		},
			.bits  = {
			.pxx = IMX8MN_MIPI_SW_Pxx_REQ,
			.map = IMX8MN_MIPI_A53_DOMAIN,
		},
		.pgc   = BIT(IMX8MN_PGC_MIPI),
	},
};

static const struct imx_pgc_domain_data imx8mn_pgc_domain_data = {
	.domains = imx8mn_pgc_domains,
	.domains_num = ARRAY_SIZE(imx8mn_pgc_domains),
	.pgc_regs = &imx7_pgc_regs,
};

static int imx_pgc_domain_probe(struct device *dev)
{
	struct imx_pgc_domain *domain = dev->priv;
	int ret;

	domain->dev = dev;

	domain->regulator = regulator_get(domain->dev, "power");
	if (IS_ERR(domain->regulator)) {
		if (PTR_ERR(domain->regulator) != -ENODEV)
			return dev_err_probe(domain->dev, PTR_ERR(domain->regulator),
					     "Failed to get domain's regulator\n");
	} else if (domain->voltage) {
		regulator_set_voltage(domain->regulator,
				      domain->voltage, domain->voltage);
	}

	domain->num_clks = clk_bulk_get_all(domain->dev, &domain->clks);
	if (domain->num_clks < 0)
		return dev_err_probe(domain->dev, domain->num_clks,
				     "Failed to get domain's clocks\n");

	/* There are no power domains yet with multiple resets */
	if (reset_control_get_count(domain->dev) > 1)
		return dev_err_probe(domain->dev, -ENOSYS,
				     "driver can't handle multiple resets yet\n");

	domain->reset = reset_control_get_optional(domain->dev, NULL);
	if (IS_ERR(domain->reset))
		return dev_err_probe(domain->dev, PTR_ERR(domain->reset),
				     "Failed to get domain's resets\n");

	if (domain->bits.map)
		regmap_update_bits(domain->regmap, domain->regs->map,
				   domain->bits.map, domain->bits.map);

	ret = pm_genpd_init(&domain->genpd, NULL, true);
	if (ret) {
		dev_err(domain->dev, "Failed to init power domain\n");
		goto out_domain_unmap;
	}

	ret = of_genpd_add_provider_simple(domain->dev->of_node,
					   &domain->genpd);
	if (ret) {
		dev_err(domain->dev, "Failed to add genpd provider\n");
		goto out_domain_unmap;
	}

	return 0;

out_domain_unmap:
	if (domain->bits.map)
		regmap_update_bits(domain->regmap, domain->regs->map,
				   domain->bits.map, 0);
	return ret;
}

static const struct platform_device_id imx_pgc_domain_id[] = {
	{ "imx-pgc-domain", },
	{ },
};

static struct driver imx_pgc_domain_driver = {
	.name = "imx-pgc",
	.probe = imx_pgc_domain_probe,
	.id_table = imx_pgc_domain_id,
};
coredevice_platform_driver(imx_pgc_domain_driver);

static int imx_gpcv2_probe(struct device *dev)
{
	const struct imx_pgc_domain_data *domain_data =
			of_device_get_match_data(dev);

	struct regmap_config regmap_config = {
		.reg_bits	= 32,
		.val_bits	= 32,
		.reg_stride	= 4,
		.max_register   = SZ_4K,
	};
	struct device_node *pgc_np, *np;
	struct resource *res;
	struct regmap *regmap;
	int ret, pass = 0;

	pgc_np = of_get_child_by_name(dev->of_node, "pgc");
	if (!pgc_np) {
		dev_err(dev, "No power domains specified in DT\n");
		return -EINVAL;
	}

	res = dev_request_mem_resource(dev, 0);
	if (IS_ERR(res))
		return PTR_ERR(res);

	regmap = regmap_init_mmio(dev, IOMEM(res->start), &regmap_config);
	if (IS_ERR(regmap))
		return dev_err_probe(dev, PTR_ERR(regmap), "failed to init regmap\n");

	/*
	 * Run two passes for the registration of the PGC domain platform
	 * devices: first all devices that are not part of a power-domain
	 * themselves, then all the others. This avoids -EPROBE_DEFER being
	 * returned for nested domains, that need their parent PGC domains
	 * to be present on probe.
	 */
again:
	for_each_child_of_node(pgc_np, np) {
		bool child_domain = of_property_read_bool(np, "power-domains");
		struct imx_pgc_domain *domain;
		struct device *pd_dev;
		u32 domain_index;

		if ((pass == 0 && child_domain) || (pass == 1 && !child_domain))
			continue;

		if (!of_device_is_available(np))
			continue;

		ret = of_property_read_u32(np, "reg", &domain_index);
		if (ret) {
			dev_err(dev, "Failed to read 'reg' property\n");
			return ret;
		}

		if (domain_index >= domain_data->domains_num) {
			dev_warn(dev,
				 "Domain index %d is out of bounds\n",
				 domain_index);
			continue;
		}

		domain = xmemdup(&domain_data->domains[domain_index],
				 sizeof(domain_data->domains[domain_index]));
		domain->regmap = regmap;
		domain->genpd.power_on = imx_pgc_power_up;
		domain->regs = domain_data->pgc_regs;
		domain->genpd.power_off = imx_pgc_power_down;

		pd_dev = xzalloc(sizeof(*pd_dev));
		pd_dev->of_node = np;
		pd_dev->of_node->dev = pd_dev;
		pd_dev->id = domain_index;
		pd_dev->parent = dev;
		pd_dev->priv = domain;
		dev_set_name(pd_dev, imx_pgc_domain_id[0].name);

		ret = platform_device_register(pd_dev);
		if (ret)
			return ret;
	}

	if (pass == 0) {
		pass++;
		goto again;
	}

	return 0;
}

static const struct of_device_id imx_gpcv2_dt_ids[] = {
	{ .compatible = "fsl,imx7d-gpc", .data = &imx7_pgc_domain_data, },
	{ .compatible = "fsl,imx8mm-gpc", .data = &imx8mm_pgc_domain_data, },
	{ .compatible = "fsl,imx8mn-gpc", .data = &imx8mn_pgc_domain_data, },
	{ .compatible = "fsl,imx8mp-gpc", .data = &imx8mp_pgc_domain_data, },
	{ .compatible = "fsl,imx8mq-gpc", .data = &imx8m_pgc_domain_data, },
	{ }
};
MODULE_DEVICE_TABLE(of, imx_gpcv2_dt_ids);

static struct driver imx_gpcv2_driver = {
	.name = "imx-gpcv2",
	.probe = imx_gpcv2_probe,
	.of_compatible = DRV_OF_COMPAT(imx_gpcv2_dt_ids),
};
coredevice_platform_driver(imx_gpcv2_driver);
