// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) STMicroelectronics 2018 - All Rights Reserved
 * Authors: Ludovic Barre <ludovic.barre@st.com> for STMicroelectronics.
 *          Fabien Dessenne <fabien.dessenne@st.com> for STMicroelectronics.
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <driver.h>
#include <init.h>
#include <io.h>
#include <mach/stm32mp/smc.h>
#include <mfd/syscon.h>
#include <of_address.h>
#include <linux/regmap.h>
#include <linux/remoteproc.h>
#include <linux/reset.h>

#include "remoteproc_internal.h"

#define HOLD_BOOT		0
#define RELEASE_BOOT		1

struct stm32_syscon {
	struct regmap *map;
	u32 reg;
	u32 mask;
};

struct stm32_rproc {
	struct reset_control *rst;
	struct reset_control *hold_boot_rst;
	struct stm32_syscon hold_boot;
	bool hold_boot_smc;
};

static void *stm32_rproc_da_to_va(struct rproc *rproc, u64 da, int len)
{
	__be32 in_addr = cpu_to_be32(da);
	struct device *dev = &rproc->dev;
	u64 paddr;

	paddr = of_translate_dma_address(dev->parent->of_node, &in_addr);
	if (paddr == OF_BAD_ADDR)
		return NULL;

	return phys_to_virt(paddr);
}

static int stm32_rproc_set_hold_boot(struct rproc *rproc, bool hold)
{
	struct stm32_rproc *ddata = rproc->priv;
	struct stm32_syscon *hold_boot = &ddata->hold_boot;
	struct arm_smccc_res smc_res;
	int val, err;

	/*
	 * Three ways to manage the hold boot
	 * - using SCMI: the hold boot is managed as a reset,
	 * - using Linux(no SCMI): the hold boot is managed as a syscon register
	 * - using SMC call (deprecated): use SMC reset interface
	 */

	val = hold ? HOLD_BOOT : RELEASE_BOOT;

	if (ddata->hold_boot_rst) {
		/* Use the SCMI reset controller */
		if (!hold)
			err = reset_control_deassert(ddata->hold_boot_rst);
		else
			err =  reset_control_assert(ddata->hold_boot_rst);
	} else if (IS_ENABLED(CONFIG_HAVE_ARM_SMCCC) && ddata->hold_boot_smc) {
		/* Use the SMC call */
		arm_smccc_smc(STM32_SMC_RCC, STM32_SMC_REG_WRITE,
			      hold_boot->reg, val, 0, 0, 0, 0, &smc_res);
		err = smc_res.a0;
	} else {
		/* Use syscon */
		err = regmap_update_bits(hold_boot->map, hold_boot->reg,
					 hold_boot->mask, val);
	}

	if (err)
		dev_err(&rproc->dev, "failed to set hold boot\n");

	return err;
}

static int stm32_rproc_start(struct rproc *rproc)
{
	int err;

	err = stm32_rproc_set_hold_boot(rproc, false);
	if (err)
		return err;

	return stm32_rproc_set_hold_boot(rproc, true);
}

static int stm32_rproc_stop(struct rproc *rproc)
{
	struct stm32_rproc *ddata = rproc->priv;
	int err;

	err = stm32_rproc_set_hold_boot(rproc, true);
	if (err)
		return err;

	err = reset_control_assert(ddata->rst);
	if (err) {
		dev_err(&rproc->dev, "failed to assert the reset\n");
		return err;
	}

	return 0;
}

static struct rproc_ops st_rproc_ops = {
	.start		= stm32_rproc_start,
	.stop		= stm32_rproc_stop,
	.da_to_va	= stm32_rproc_da_to_va,
};

static int stm32_rproc_get_syscon(struct device_node *np, const char *prop,
				  struct stm32_syscon *syscon)
{
	int err = 0;

	syscon->map = syscon_regmap_lookup_by_phandle(np, prop);
	if (IS_ERR(syscon->map)) {
		err = PTR_ERR(syscon->map);
		syscon->map = NULL;
		goto out;
	}

	err = of_property_read_u32_index(np, prop, 1, &syscon->reg);
	if (err)
		goto out;

	err = of_property_read_u32_index(np, prop, 2, &syscon->mask);

out:
	return err;
}

static int stm32_rproc_parse_dt(struct device *dev, struct stm32_rproc *ddata)
{
	struct device_node *np = dev->of_node;
	struct stm32_syscon tz;
	unsigned int tzen;
	int err;

	ddata->rst = reset_control_get(dev, NULL);
	if (IS_ERR(ddata->rst)) {
		dev_err(dev, "failed to get mcu reset\n");
		return PTR_ERR(ddata->rst);
	}

	/*
	 * Three ways to manage the hold boot
	 * - using SCMI: the hold boot is managed as a reset
	 *    The DT "reset-mames" property should be defined with 2 items:
	 *        reset-names = "mcu_rst", "hold_boot";
	 * - using SMC call (deprecated): use SMC reset interface
	 *    The DT "reset-mames" property is optional, "st,syscfg-tz" is required
	 * - default(no SCMI, no SMC): the hold boot is managed as a syscon register
	 *    The DT "reset-mames" property is optional, "st,syscfg-holdboot" is required
	 */

	ddata->hold_boot_rst = reset_control_get_optional(dev, "hold_boot");
	if (IS_ERR(ddata->hold_boot_rst))
		return dev_err_probe(dev, PTR_ERR(ddata->hold_boot_rst),
				     "failed to get hold_boot reset\n");

	if (!ddata->hold_boot_rst && IS_ENABLED(CONFIG_HAVE_ARM_SMCCC)) {
		/* Manage the MCU_BOOT using SMC call */
		err = stm32_rproc_get_syscon(np, "st,syscfg-tz", &tz);
		if (!err) {
			err = regmap_read(tz.map, tz.reg, &tzen);
			if (err) {
				dev_err(dev, "failed to read tzen\n");
				return err;
			}
			ddata->hold_boot_smc = tzen & tz.mask;
		}
	}

	if (!ddata->hold_boot_rst && !ddata->hold_boot_smc) {
		/* Default: hold boot manage it through the syscon controller */
		err = stm32_rproc_get_syscon(np, "st,syscfg-holdboot",
					     &ddata->hold_boot);
		if (err) {
			dev_err(dev, "failed to get hold boot\n");
			return err;
		}
	}

	return 0;
}

static int stm32_rproc_probe(struct device *dev)
{
	struct rproc *rproc;
	int ret;

	rproc = rproc_alloc(dev, dev_name(dev), &st_rproc_ops,
			    sizeof(struct stm32_rproc));
	if (!rproc)
		return -ENOMEM;

	ret = stm32_rproc_parse_dt(dev, rproc->priv);
	if (ret)
		return ret;

	return rproc_add(rproc);
}

static const struct of_device_id stm32_rproc_of_match[] = {
	{ .compatible = "st,stm32mp1-m4" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, stm32_rproc_of_match);

static struct driver stm32_rproc_driver = {
	.name = "stm32-rproc",
	.probe = stm32_rproc_probe,
	.of_compatible = DRV_OF_COMPAT(stm32_rproc_of_match),
};
device_platform_driver(stm32_rproc_driver);
