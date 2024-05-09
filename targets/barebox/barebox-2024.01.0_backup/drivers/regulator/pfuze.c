// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017 Sascha Hauer, Pengutronix
 */

#include <common.h>
#include <init.h>
#include <driver.h>
#include <xfuncs.h>
#include <errno.h>
#include <malloc.h>
#include <of.h>
#include <linux/regmap.h>
#include <mfd/pfuze.h>

#include <i2c/i2c.h>

#include <poweroff.h>

#define DRIVERNAME		"pfuze"

#define PFUZE100_SW1ABMODE	0x23
#define PFUZE100_SW1CMODE	0x31
#define PFUZE100_SW2MODE	0x38
#define PFUZE100_SW3AMODE	0x3f
#define PFUZE100_SW3BMODE	0x46
#define PFUZE100_SW4MODE	0x4d
#define PFUZE100_VGEN1VOL	0x6c
#define PFUZE100_VGEN2VOL	0x6d
#define PFUZE100_VGEN3VOL	0x6e
#define PFUZE100_VGEN4VOL	0x6f
#define PFUZE100_VGEN5VOL	0x70
#define PFUZE100_VGEN6VOL	0x71

#define PFUZE100_SWxMODE_MASK	0xf
#define PFUZE100_SWxMODE_APS_APS	0x8
#define PFUZE100_SWxMODE_APS_OFF	0x4

#define PFUZE100_VGENxLPWR	BIT(6)
#define PFUZE100_VGENxSTBY	BIT(5)

struct pfuze {
	struct device			*dev;
	struct regmap			*map;
	struct i2c_client		*client;
	int				revision;
};

struct pfuze_devtype {
	int	(*revision)(struct pfuze*);
};

#define to_pfuze(a)		container_of(a, struct pfuze, cdev)

static struct pfuze *pfuze_dev;

static void(*pfuze_init_callback)(struct regmap *map);

int pfuze_register_init_callback(void(*callback)(struct regmap *map))
{
	if (pfuze_init_callback)
		return -EBUSY;

	pfuze_init_callback = callback;

	if (pfuze_dev)
		pfuze_init_callback(pfuze_dev->map);

	return 0;
}

static int pfuze_i2c_reg_read(void *ctx, unsigned int reg, unsigned int *val)
{
	struct pfuze *pfuze = ctx;
	u8 buf[1];
	int ret;

	ret = i2c_read_reg(pfuze->client, reg, buf, 1);
	*val = buf[0];

	return ret == 1 ? 0 : ret;
}

static int pfuze_i2c_reg_write(void *ctx, unsigned int reg, unsigned int val)
{
	struct pfuze *pfuze = ctx;
	u8 buf[] = {
		val & 0xff,
	};
	int ret;

	ret = i2c_write_reg(pfuze->client, reg, buf, 1);

	return ret == 1 ? 0 : ret;
}

static void pfuze_power_off_prepare(struct poweroff_handler *handler)
{
	dev_info(pfuze_dev->dev, "Configure standy mode for power off");

	/* Switch from default mode: APS/APS to APS/Off */
	regmap_write_bits(pfuze_dev->map, PFUZE100_SW1ABMODE,
			  PFUZE100_SWxMODE_MASK, PFUZE100_SWxMODE_APS_OFF);
	regmap_write_bits(pfuze_dev->map, PFUZE100_SW1CMODE,
			  PFUZE100_SWxMODE_MASK, PFUZE100_SWxMODE_APS_OFF);
	regmap_write_bits(pfuze_dev->map, PFUZE100_SW2MODE,
			  PFUZE100_SWxMODE_MASK, PFUZE100_SWxMODE_APS_OFF);
	regmap_write_bits(pfuze_dev->map, PFUZE100_SW3AMODE,
			  PFUZE100_SWxMODE_MASK, PFUZE100_SWxMODE_APS_OFF);
	regmap_write_bits(pfuze_dev->map, PFUZE100_SW3BMODE,
			  PFUZE100_SWxMODE_MASK, PFUZE100_SWxMODE_APS_OFF);
	regmap_write_bits(pfuze_dev->map, PFUZE100_SW4MODE,
			  PFUZE100_SWxMODE_MASK, PFUZE100_SWxMODE_APS_OFF);

	regmap_write_bits(pfuze_dev->map, PFUZE100_VGEN1VOL,
			  PFUZE100_VGENxLPWR | PFUZE100_VGENxSTBY,
			  PFUZE100_VGENxSTBY);
	regmap_write_bits(pfuze_dev->map, PFUZE100_VGEN2VOL,
			  PFUZE100_VGENxLPWR | PFUZE100_VGENxSTBY,
			  PFUZE100_VGENxSTBY);
	regmap_write_bits(pfuze_dev->map, PFUZE100_VGEN3VOL,
			  PFUZE100_VGENxLPWR | PFUZE100_VGENxSTBY,
			  PFUZE100_VGENxSTBY);
	regmap_write_bits(pfuze_dev->map, PFUZE100_VGEN4VOL,
			  PFUZE100_VGENxLPWR | PFUZE100_VGENxSTBY,
			  PFUZE100_VGENxSTBY);
	regmap_write_bits(pfuze_dev->map, PFUZE100_VGEN5VOL,
			  PFUZE100_VGENxLPWR | PFUZE100_VGENxSTBY,
			  PFUZE100_VGENxSTBY);
	regmap_write_bits(pfuze_dev->map, PFUZE100_VGEN6VOL,
			  PFUZE100_VGENxLPWR | PFUZE100_VGENxSTBY,
			  PFUZE100_VGENxSTBY);
}

static struct regmap_bus regmap_pfuze_i2c_bus = {
	.reg_write = pfuze_i2c_reg_write,
	.reg_read = pfuze_i2c_reg_read,
};

static const struct regmap_config pfuze_regmap_i2c_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 127,
};

static int __init pfuze_probe(struct device *dev)
{
	int ret;

	if (pfuze_dev)
		return -EBUSY;

	pfuze_dev = xzalloc(sizeof(*pfuze_dev));
	pfuze_dev->dev = dev;

	pfuze_dev->client = to_i2c_client(dev);
	pfuze_dev->map = regmap_init(dev, &regmap_pfuze_i2c_bus,
				     pfuze_dev, &pfuze_regmap_i2c_config);

	ret = regmap_register_cdev(pfuze_dev->map, NULL);
	if (ret)
		return ret;

	if (pfuze_init_callback)
		pfuze_init_callback(pfuze_dev->map);

	if (of_property_read_bool(dev->of_node,
				  "fsl,pmic-stby-poweroff"))
		return poweroff_handler_register_fn(pfuze_power_off_prepare);

	return 0;
}

static struct platform_device_id pfuze_ids[] = {
	{ .name = "pfuze100" },
	{ .name = "pfuze200" },
	{ .name = "pfuze3000" },
	{ .name = "pfuze3001" },
	{ }
};

static __maybe_unused struct of_device_id pfuze_dt_ids[] = {
	{ .compatible = "fsl,pfuze100" },
	{ .compatible = "fsl,pfuze200" },
	{ .compatible = "fsl,pfuze3000" },
	{ .compatible = "fsl,pfuze3001" },
	{ }
};
MODULE_DEVICE_TABLE(of, pfuze_dt_ids);

static struct driver pfuze_i2c_driver = {
	.name		= "pfuze-i2c",
	.probe		= pfuze_probe,
	.id_table	= pfuze_ids,
	.of_compatible	= DRV_OF_COMPAT(pfuze_dt_ids),
};

device_i2c_driver(pfuze_i2c_driver);
