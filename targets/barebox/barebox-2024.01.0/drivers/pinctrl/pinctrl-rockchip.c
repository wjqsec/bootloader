// SPDX-License-Identifier: GPL-2.0-only
/*
 * Rockchip pinctrl and gpio driver for Barebox
 *
 * Copyright (C) 2014 Beniamino Galvani <b.galvani@gmail.com>
 *
 * Based on Linux pinctrl-rockchip:
 *   Copyright (C) 2013 MundoReader S.L.
 *   Copyright (C) 2012 Samsung Electronics Co., Ltd.
 *   Copyright (C) 2012 Linaro Ltd
 *   Copyright (C) 2011-2012 Jean-Christophe PLAGNIOL-VILLARD
 */

#include <common.h>
#include <gpio.h>
#include <init.h>
#include <malloc.h>
#include <mfd/syscon.h>
#include <linux/regmap.h>
#include <of.h>
#include <of_address.h>
#include <pinctrl.h>
#include <dt-bindings/pinctrl/rockchip.h>

#include <linux/basic_mmio_gpio.h>
#include <linux/clk.h>
#include <linux/err.h>

#include "pinctrl-rockchip.h"

/*
 * Generate a bitmask for setting a value (v) with a write mask bit in hiword
 * register 31:16 area.
 */
#define WRITE_MASK_VAL(h, l, v) \
	(GENMASK(((h) + 16), ((l) + 16)) | (((v) << (l)) & GENMASK((h), (l))))

/*
 * Encode variants of iomux registers into a type variable
 */
#define IOMUX_GPIO_ONLY		BIT(0)
#define IOMUX_WIDTH_4BIT	BIT(1)
#define IOMUX_SOURCE_PMU	BIT(2)
#define IOMUX_UNROUTED		BIT(3)
#define IOMUX_WIDTH_3BIT	BIT(4)
#define IOMUX_WIDTH_2BIT	BIT(5)
#define IOMUX_L_SOURCE_PMU	BIT(6)

#define PIN_BANK(id, pins, label)			\
	{						\
		.bank_num	= id,			\
		.nr_pins	= pins,			\
		.name		= label,		\
		.iomux		= {			\
			{ .offset = -1 },		\
			{ .offset = -1 },		\
			{ .offset = -1 },		\
			{ .offset = -1 },		\
		},					\
	}

#define PIN_BANK_IOMUX_FLAGS(id, pins, label, iom0, iom1, iom2, iom3)	\
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .type = iom0, .offset = -1 },			\
			{ .type = iom1, .offset = -1 },			\
			{ .type = iom2, .offset = -1 },			\
			{ .type = iom3, .offset = -1 },			\
		},							\
	}

#define PIN_BANK_DRV_FLAGS(id, pins, label, type0, type1, type2, type3) \
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .offset = -1 },				\
			{ .offset = -1 },				\
			{ .offset = -1 },				\
			{ .offset = -1 },				\
		},							\
		.drv		= {					\
			{ .drv_type = type0, .offset = -1 },		\
			{ .drv_type = type1, .offset = -1 },		\
			{ .drv_type = type2, .offset = -1 },		\
			{ .drv_type = type3, .offset = -1 },		\
		},							\
	}

#define PIN_BANK_IOMUX_FLAGS_PULL_FLAGS(id, pins, label, iom0, iom1,	\
					iom2, iom3, pull0, pull1,	\
					pull2, pull3)			\
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .type = iom0, .offset = -1 },			\
			{ .type = iom1, .offset = -1 },			\
			{ .type = iom2, .offset = -1 },			\
			{ .type = iom3, .offset = -1 },			\
		},							\
		.pull_type[0] = pull0,					\
		.pull_type[1] = pull1,					\
		.pull_type[2] = pull2,					\
		.pull_type[3] = pull3,					\
	}

#define PIN_BANK_DRV_FLAGS_PULL_FLAGS(id, pins, label, drv0, drv1,	\
				      drv2, drv3, pull0, pull1,		\
				      pull2, pull3)			\
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .offset = -1 },				\
			{ .offset = -1 },				\
			{ .offset = -1 },				\
			{ .offset = -1 },				\
		},							\
		.drv		= {					\
			{ .drv_type = drv0, .offset = -1 },		\
			{ .drv_type = drv1, .offset = -1 },		\
			{ .drv_type = drv2, .offset = -1 },		\
			{ .drv_type = drv3, .offset = -1 },		\
		},							\
		.pull_type[0] = pull0,					\
		.pull_type[1] = pull1,					\
		.pull_type[2] = pull2,					\
		.pull_type[3] = pull3,					\
	}

#define PIN_BANK_IOMUX_FLAGS_OFFSET(id, pins, label, iom0, iom1, iom2,	\
				    iom3, offset0, offset1, offset2,	\
				    offset3)				\
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .type = iom0, .offset = offset0 },		\
			{ .type = iom1, .offset = offset1 },		\
			{ .type = iom2, .offset = offset2 },		\
			{ .type = iom3, .offset = offset3 },		\
		},							\
	}

#define PIN_BANK_IOMUX_DRV_FLAGS_OFFSET(id, pins, label, iom0, iom1,	\
					iom2, iom3, drv0, drv1, drv2,	\
					drv3, offset0, offset1,		\
					offset2, offset3)		\
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .type = iom0, .offset = -1 },			\
			{ .type = iom1, .offset = -1 },			\
			{ .type = iom2, .offset = -1 },			\
			{ .type = iom3, .offset = -1 },			\
		},							\
		.drv		= {					\
			{ .drv_type = drv0, .offset = offset0 },	\
			{ .drv_type = drv1, .offset = offset1 },	\
			{ .drv_type = drv2, .offset = offset2 },	\
			{ .drv_type = drv3, .offset = offset3 },	\
		},							\
	}

#define PIN_BANK_IOMUX_FLAGS_DRV_FLAGS_OFFSET_PULL_FLAGS(id, pins,	\
					      label, iom0, iom1, iom2,  \
					      iom3, drv0, drv1, drv2,   \
					      drv3, offset0, offset1,   \
					      offset2, offset3, pull0,  \
					      pull1, pull2, pull3)	\
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .type = iom0, .offset = -1 },			\
			{ .type = iom1, .offset = -1 },			\
			{ .type = iom2, .offset = -1 },			\
			{ .type = iom3, .offset = -1 },			\
		},							\
		.drv		= {					\
			{ .drv_type = drv0, .offset = offset0 },	\
			{ .drv_type = drv1, .offset = offset1 },	\
			{ .drv_type = drv2, .offset = offset2 },	\
			{ .drv_type = drv3, .offset = offset3 },	\
		},							\
		.pull_type[0] = pull0,					\
		.pull_type[1] = pull1,					\
		.pull_type[2] = pull2,					\
		.pull_type[3] = pull3,					\
	}

#define PIN_BANK_MUX_ROUTE_FLAGS(ID, PIN, FUNC, REG, VAL, FLAG)		\
	{								\
		.bank_num	= ID,					\
		.pin		= PIN,					\
		.func		= FUNC,					\
		.route_offset	= REG,					\
		.route_val	= VAL,					\
		.route_location	= FLAG,					\
	}

#define RK_MUXROUTE_SAME(ID, PIN, FUNC, REG, VAL)	\
	PIN_BANK_MUX_ROUTE_FLAGS(ID, PIN, FUNC, REG, VAL, ROCKCHIP_ROUTE_SAME)

#define RK_MUXROUTE_GRF(ID, PIN, FUNC, REG, VAL)	\
	PIN_BANK_MUX_ROUTE_FLAGS(ID, PIN, FUNC, REG, VAL, ROCKCHIP_ROUTE_GRF)

#define RK_MUXROUTE_PMU(ID, PIN, FUNC, REG, VAL)	\
	PIN_BANK_MUX_ROUTE_FLAGS(ID, PIN, FUNC, REG, VAL, ROCKCHIP_ROUTE_PMU)

#define RK3588_PIN_BANK_FLAGS(ID, PIN, LABEL, M, P)			\
	PIN_BANK_IOMUX_FLAGS_PULL_FLAGS(ID, PIN, LABEL, M, M, M, M, P, P, P, P)

enum {
	RK_BIAS_DISABLE = 0,
	RK_BIAS_PULL_UP,
	RK_BIAS_PULL_DOWN,
	RK_BIAS_BUS_HOLD,
};

static struct rockchip_pinctrl *to_rockchip_pinctrl(struct pinctrl_device *pdev)
{
	return container_of(pdev, struct rockchip_pinctrl, pctl_dev);
}

static int parse_bias_config(struct device_node *np)
{
	u32 val;
 
	if (of_property_read_u32(np, "bias-pull-up", &val) != -EINVAL)
		return RK_BIAS_PULL_UP;
	else if (of_property_read_u32(np, "bias-pull-down", &val) != -EINVAL)
		return RK_BIAS_PULL_DOWN;
	else if (of_property_read_u32(np, "bias-bus-hold", &val) != -EINVAL)
		return RK_BIAS_BUS_HOLD;
	else
		return RK_BIAS_DISABLE;
}

static struct rockchip_pin_bank *bank_num_to_bank(
					struct rockchip_pinctrl *info,
					unsigned num)
{
	struct rockchip_pin_bank *b = info->ctrl->pin_banks;
	int i;

	for (i = 0; i < info->ctrl->nr_banks; i++, b++) {
		if (b->bank_num == num)
			return b;
	}

	return ERR_PTR(-EINVAL);
}

/*
 * Hardware access
 */

static struct rockchip_mux_recalced_data rv1108_mux_recalced_data[] = {
	{
		.num = 1,
		.pin = 0,
		.reg = 0x418,
		.bit = 0,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 1,
		.reg = 0x418,
		.bit = 2,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 2,
		.reg = 0x418,
		.bit = 4,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 3,
		.reg = 0x418,
		.bit = 6,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 4,
		.reg = 0x418,
		.bit = 8,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 5,
		.reg = 0x418,
		.bit = 10,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 6,
		.reg = 0x418,
		.bit = 12,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 7,
		.reg = 0x418,
		.bit = 14,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 8,
		.reg = 0x41c,
		.bit = 0,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 9,
		.reg = 0x41c,
		.bit = 2,
		.mask = 0x3
	},
};

static struct rockchip_mux_recalced_data rv1126_mux_recalced_data[] = {
	{
		.num = 0,
		.pin = 20,
		.reg = 0x10000,
		.bit = 0,
		.mask = 0xf
	},
	{
		.num = 0,
		.pin = 21,
		.reg = 0x10000,
		.bit = 4,
		.mask = 0xf
	},
	{
		.num = 0,
		.pin = 22,
		.reg = 0x10000,
		.bit = 8,
		.mask = 0xf
	},
	{
		.num = 0,
		.pin = 23,
		.reg = 0x10000,
		.bit = 12,
		.mask = 0xf
	},
};

static  struct rockchip_mux_recalced_data rk3128_mux_recalced_data[] = {
	{
		.num = 2,
		.pin = 20,
		.reg = 0xe8,
		.bit = 0,
		.mask = 0x7
	}, {
		.num = 2,
		.pin = 21,
		.reg = 0xe8,
		.bit = 4,
		.mask = 0x7
	}, {
		.num = 2,
		.pin = 22,
		.reg = 0xe8,
		.bit = 8,
		.mask = 0x7
	}, {
		.num = 2,
		.pin = 23,
		.reg = 0xe8,
		.bit = 12,
		.mask = 0x7
	}, {
		.num = 2,
		.pin = 24,
		.reg = 0xd4,
		.bit = 12,
		.mask = 0x7
	},
};

static struct rockchip_mux_recalced_data rk3308_mux_recalced_data[] = {
	{
		/* gpio1b6_sel */
		.num = 1,
		.pin = 14,
		.reg = 0x28,
		.bit = 12,
		.mask = 0xf
	}, {
		/* gpio1b7_sel */
		.num = 1,
		.pin = 15,
		.reg = 0x2c,
		.bit = 0,
		.mask = 0x3
	}, {
		/* gpio1c2_sel */
		.num = 1,
		.pin = 18,
		.reg = 0x30,
		.bit = 4,
		.mask = 0xf
	}, {
		/* gpio1c3_sel */
		.num = 1,
		.pin = 19,
		.reg = 0x30,
		.bit = 8,
		.mask = 0xf
	}, {
		/* gpio1c4_sel */
		.num = 1,
		.pin = 20,
		.reg = 0x30,
		.bit = 12,
		.mask = 0xf
	}, {
		/* gpio1c5_sel */
		.num = 1,
		.pin = 21,
		.reg = 0x34,
		.bit = 0,
		.mask = 0xf
	}, {
		/* gpio1c6_sel */
		.num = 1,
		.pin = 22,
		.reg = 0x34,
		.bit = 4,
		.mask = 0xf
	}, {
		/* gpio1c7_sel */
		.num = 1,
		.pin = 23,
		.reg = 0x34,
		.bit = 8,
		.mask = 0xf
	}, {
		/* gpio2a2_sel */
		.num = 2,
		.pin = 2,
		.reg = 0x40,
		.bit = 4,
		.mask = 0x3
	}, {
		/* gpio2a3_sel */
		.num = 2,
		.pin = 3,
		.reg = 0x40,
		.bit = 6,
		.mask = 0x3
	}, {
		/* gpio2c0_sel */
		.num = 2,
		.pin = 16,
		.reg = 0x50,
		.bit = 0,
		.mask = 0x3
	}, {
		/* gpio3b2_sel */
		.num = 3,
		.pin = 10,
		.reg = 0x68,
		.bit = 4,
		.mask = 0x3
	}, {
		/* gpio3b3_sel */
		.num = 3,
		.pin = 11,
		.reg = 0x68,
		.bit = 6,
		.mask = 0x3
	}, {
		/* gpio3b4_sel */
		.num = 3,
		.pin = 12,
		.reg = 0x68,
		.bit = 8,
		.mask = 0xf
	}, {
		/* gpio3b5_sel */
		.num = 3,
		.pin = 13,
		.reg = 0x68,
		.bit = 12,
		.mask = 0xf
	},
};

static struct rockchip_mux_recalced_data rk3328_mux_recalced_data[] = {
	{
		.num = 2,
		.pin = 12,
		.reg = 0x24,
		.bit = 8,
		.mask = 0x3
	}, {
		.num = 2,
		.pin = 15,
		.reg = 0x28,
		.bit = 0,
		.mask = 0x7
	}, {
		.num = 2,
		.pin = 23,
		.reg = 0x30,
		.bit = 14,
		.mask = 0x3
	},
};

static void rockchip_get_recalced_mux(struct rockchip_pin_bank *bank, int pin,
				      int *reg, u8 *bit, int *mask)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct rockchip_mux_recalced_data *data = NULL;
	int i;

	for (i = 0; i < ctrl->niomux_recalced; i++) {
		data = &ctrl->iomux_recalced[i];
		if (data->num == bank->bank_num &&
		    data->pin == pin)
			break;
	}

	if (i >= ctrl->niomux_recalced)
		return;

	*reg = data->reg;
	*mask = data->mask;
	*bit = data->bit;
}

static struct rockchip_mux_route_data px30_mux_route_data[] = {
	RK_MUXROUTE_SAME(2, RK_PB4, 1, 0x184, BIT(16 + 7)), /* cif-d0m0 */
	RK_MUXROUTE_SAME(3, RK_PA1, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-d0m1 */
	RK_MUXROUTE_SAME(2, RK_PB6, 1, 0x184, BIT(16 + 7)), /* cif-d1m0 */
	RK_MUXROUTE_SAME(3, RK_PA2, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-d1m1 */
	RK_MUXROUTE_SAME(2, RK_PA0, 1, 0x184, BIT(16 + 7)), /* cif-d2m0 */
	RK_MUXROUTE_SAME(3, RK_PA3, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-d2m1 */
	RK_MUXROUTE_SAME(2, RK_PA1, 1, 0x184, BIT(16 + 7)), /* cif-d3m0 */
	RK_MUXROUTE_SAME(3, RK_PA5, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-d3m1 */
	RK_MUXROUTE_SAME(2, RK_PA2, 1, 0x184, BIT(16 + 7)), /* cif-d4m0 */
	RK_MUXROUTE_SAME(3, RK_PA7, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-d4m1 */
	RK_MUXROUTE_SAME(2, RK_PA3, 1, 0x184, BIT(16 + 7)), /* cif-d5m0 */
	RK_MUXROUTE_SAME(3, RK_PB0, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-d5m1 */
	RK_MUXROUTE_SAME(2, RK_PA4, 1, 0x184, BIT(16 + 7)), /* cif-d6m0 */
	RK_MUXROUTE_SAME(3, RK_PB1, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-d6m1 */
	RK_MUXROUTE_SAME(2, RK_PA5, 1, 0x184, BIT(16 + 7)), /* cif-d7m0 */
	RK_MUXROUTE_SAME(3, RK_PB4, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-d7m1 */
	RK_MUXROUTE_SAME(2, RK_PA6, 1, 0x184, BIT(16 + 7)), /* cif-d8m0 */
	RK_MUXROUTE_SAME(3, RK_PB6, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-d8m1 */
	RK_MUXROUTE_SAME(2, RK_PA7, 1, 0x184, BIT(16 + 7)), /* cif-d9m0 */
	RK_MUXROUTE_SAME(3, RK_PB7, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-d9m1 */
	RK_MUXROUTE_SAME(2, RK_PB7, 1, 0x184, BIT(16 + 7)), /* cif-d10m0 */
	RK_MUXROUTE_SAME(3, RK_PC6, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-d10m1 */
	RK_MUXROUTE_SAME(2, RK_PC0, 1, 0x184, BIT(16 + 7)), /* cif-d11m0 */
	RK_MUXROUTE_SAME(3, RK_PC7, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-d11m1 */
	RK_MUXROUTE_SAME(2, RK_PB0, 1, 0x184, BIT(16 + 7)), /* cif-vsyncm0 */
	RK_MUXROUTE_SAME(3, RK_PD1, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-vsyncm1 */
	RK_MUXROUTE_SAME(2, RK_PB1, 1, 0x184, BIT(16 + 7)), /* cif-hrefm0 */
	RK_MUXROUTE_SAME(3, RK_PD2, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-hrefm1 */
	RK_MUXROUTE_SAME(2, RK_PB2, 1, 0x184, BIT(16 + 7)), /* cif-clkinm0 */
	RK_MUXROUTE_SAME(3, RK_PD3, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-clkinm1 */
	RK_MUXROUTE_SAME(2, RK_PB3, 1, 0x184, BIT(16 + 7)), /* cif-clkoutm0 */
	RK_MUXROUTE_SAME(3, RK_PD0, 3, 0x184, BIT(16 + 7) | BIT(7)), /* cif-clkoutm1 */
	RK_MUXROUTE_SAME(3, RK_PC6, 2, 0x184, BIT(16 + 8)), /* pdm-m0 */
	RK_MUXROUTE_SAME(2, RK_PC6, 1, 0x184, BIT(16 + 8) | BIT(8)), /* pdm-m1 */
	RK_MUXROUTE_SAME(3, RK_PD3, 2, 0x184, BIT(16 + 8)), /* pdm-sdi0m0 */
	RK_MUXROUTE_SAME(2, RK_PC5, 2, 0x184, BIT(16 + 8) | BIT(8)), /* pdm-sdi0m1 */
	RK_MUXROUTE_SAME(1, RK_PD3, 2, 0x184, BIT(16 + 10)), /* uart2-rxm0 */
	RK_MUXROUTE_SAME(2, RK_PB6, 2, 0x184, BIT(16 + 10) | BIT(10)), /* uart2-rxm1 */
	RK_MUXROUTE_SAME(1, RK_PD2, 2, 0x184, BIT(16 + 10)), /* uart2-txm0 */
	RK_MUXROUTE_SAME(2, RK_PB4, 2, 0x184, BIT(16 + 10) | BIT(10)), /* uart2-txm1 */
	RK_MUXROUTE_SAME(0, RK_PC1, 2, 0x184, BIT(16 + 9)), /* uart3-rxm0 */
	RK_MUXROUTE_SAME(1, RK_PB7, 2, 0x184, BIT(16 + 9) | BIT(9)), /* uart3-rxm1 */
	RK_MUXROUTE_SAME(0, RK_PC0, 2, 0x184, BIT(16 + 9)), /* uart3-txm0 */
	RK_MUXROUTE_SAME(1, RK_PB6, 2, 0x184, BIT(16 + 9) | BIT(9)), /* uart3-txm1 */
	RK_MUXROUTE_SAME(0, RK_PC2, 2, 0x184, BIT(16 + 9)), /* uart3-ctsm0 */
	RK_MUXROUTE_SAME(1, RK_PB4, 2, 0x184, BIT(16 + 9) | BIT(9)), /* uart3-ctsm1 */
	RK_MUXROUTE_SAME(0, RK_PC3, 2, 0x184, BIT(16 + 9)), /* uart3-rtsm0 */
	RK_MUXROUTE_SAME(1, RK_PB5, 2, 0x184, BIT(16 + 9) | BIT(9)), /* uart3-rtsm1 */
};

static struct rockchip_mux_route_data rv1126_mux_route_data[] = {
	RK_MUXROUTE_GRF(3, RK_PD2, 1, 0x10260, WRITE_MASK_VAL(0, 0, 0)), /* I2S0_MCLK_M0 */
	RK_MUXROUTE_GRF(3, RK_PB0, 3, 0x10260, WRITE_MASK_VAL(0, 0, 1)), /* I2S0_MCLK_M1 */

	RK_MUXROUTE_GRF(0, RK_PD4, 4, 0x10260, WRITE_MASK_VAL(3, 2, 0)), /* I2S1_MCLK_M0 */
	RK_MUXROUTE_GRF(1, RK_PD5, 2, 0x10260, WRITE_MASK_VAL(3, 2, 1)), /* I2S1_MCLK_M1 */
	RK_MUXROUTE_GRF(2, RK_PC7, 6, 0x10260, WRITE_MASK_VAL(3, 2, 2)), /* I2S1_MCLK_M2 */

	RK_MUXROUTE_GRF(1, RK_PD0, 1, 0x10260, WRITE_MASK_VAL(4, 4, 0)), /* I2S2_MCLK_M0 */
	RK_MUXROUTE_GRF(2, RK_PB3, 2, 0x10260, WRITE_MASK_VAL(4, 4, 1)), /* I2S2_MCLK_M1 */

	RK_MUXROUTE_GRF(3, RK_PD4, 2, 0x10260, WRITE_MASK_VAL(12, 12, 0)), /* PDM_CLK0_M0 */
	RK_MUXROUTE_GRF(3, RK_PC0, 3, 0x10260, WRITE_MASK_VAL(12, 12, 1)), /* PDM_CLK0_M1 */

	RK_MUXROUTE_GRF(3, RK_PC6, 1, 0x10264, WRITE_MASK_VAL(0, 0, 0)), /* CIF_CLKOUT_M0 */
	RK_MUXROUTE_GRF(2, RK_PD1, 3, 0x10264, WRITE_MASK_VAL(0, 0, 1)), /* CIF_CLKOUT_M1 */

	RK_MUXROUTE_GRF(3, RK_PA4, 5, 0x10264, WRITE_MASK_VAL(5, 4, 0)), /* I2C3_SCL_M0 */
	RK_MUXROUTE_GRF(2, RK_PD4, 7, 0x10264, WRITE_MASK_VAL(5, 4, 1)), /* I2C3_SCL_M1 */
	RK_MUXROUTE_GRF(1, RK_PD6, 3, 0x10264, WRITE_MASK_VAL(5, 4, 2)), /* I2C3_SCL_M2 */

	RK_MUXROUTE_GRF(3, RK_PA0, 7, 0x10264, WRITE_MASK_VAL(6, 6, 0)), /* I2C4_SCL_M0 */
	RK_MUXROUTE_GRF(4, RK_PA0, 4, 0x10264, WRITE_MASK_VAL(6, 6, 1)), /* I2C4_SCL_M1 */

	RK_MUXROUTE_GRF(2, RK_PA5, 7, 0x10264, WRITE_MASK_VAL(9, 8, 0)), /* I2C5_SCL_M0 */
	RK_MUXROUTE_GRF(3, RK_PB0, 5, 0x10264, WRITE_MASK_VAL(9, 8, 1)), /* I2C5_SCL_M1 */
	RK_MUXROUTE_GRF(1, RK_PD0, 4, 0x10264, WRITE_MASK_VAL(9, 8, 2)), /* I2C5_SCL_M2 */

	RK_MUXROUTE_GRF(3, RK_PC0, 5, 0x10264, WRITE_MASK_VAL(11, 10, 0)), /* SPI1_CLK_M0 */
	RK_MUXROUTE_GRF(1, RK_PC6, 3, 0x10264, WRITE_MASK_VAL(11, 10, 1)), /* SPI1_CLK_M1 */
	RK_MUXROUTE_GRF(2, RK_PD5, 6, 0x10264, WRITE_MASK_VAL(11, 10, 2)), /* SPI1_CLK_M2 */

	RK_MUXROUTE_GRF(3, RK_PC0, 2, 0x10264, WRITE_MASK_VAL(12, 12, 0)), /* RGMII_CLK_M0 */
	RK_MUXROUTE_GRF(2, RK_PB7, 2, 0x10264, WRITE_MASK_VAL(12, 12, 1)), /* RGMII_CLK_M1 */

	RK_MUXROUTE_GRF(3, RK_PA1, 3, 0x10264, WRITE_MASK_VAL(13, 13, 0)), /* CAN_TXD_M0 */
	RK_MUXROUTE_GRF(3, RK_PA7, 5, 0x10264, WRITE_MASK_VAL(13, 13, 1)), /* CAN_TXD_M1 */

	RK_MUXROUTE_GRF(3, RK_PA4, 6, 0x10268, WRITE_MASK_VAL(0, 0, 0)), /* PWM8_M0 */
	RK_MUXROUTE_GRF(2, RK_PD7, 5, 0x10268, WRITE_MASK_VAL(0, 0, 1)), /* PWM8_M1 */

	RK_MUXROUTE_GRF(3, RK_PA5, 6, 0x10268, WRITE_MASK_VAL(2, 2, 0)), /* PWM9_M0 */
	RK_MUXROUTE_GRF(2, RK_PD6, 5, 0x10268, WRITE_MASK_VAL(2, 2, 1)), /* PWM9_M1 */

	RK_MUXROUTE_GRF(3, RK_PA6, 6, 0x10268, WRITE_MASK_VAL(4, 4, 0)), /* PWM10_M0 */
	RK_MUXROUTE_GRF(2, RK_PD5, 5, 0x10268, WRITE_MASK_VAL(4, 4, 1)), /* PWM10_M1 */

	RK_MUXROUTE_GRF(3, RK_PA7, 6, 0x10268, WRITE_MASK_VAL(6, 6, 0)), /* PWM11_IR_M0 */
	RK_MUXROUTE_GRF(3, RK_PA1, 5, 0x10268, WRITE_MASK_VAL(6, 6, 1)), /* PWM11_IR_M1 */

	RK_MUXROUTE_GRF(1, RK_PA5, 3, 0x10268, WRITE_MASK_VAL(8, 8, 0)), /* UART2_TX_M0 */
	RK_MUXROUTE_GRF(3, RK_PA2, 1, 0x10268, WRITE_MASK_VAL(8, 8, 1)), /* UART2_TX_M1 */

	RK_MUXROUTE_GRF(3, RK_PC6, 3, 0x10268, WRITE_MASK_VAL(11, 10, 0)), /* UART3_TX_M0 */
	RK_MUXROUTE_GRF(1, RK_PA7, 2, 0x10268, WRITE_MASK_VAL(11, 10, 1)), /* UART3_TX_M1 */
	RK_MUXROUTE_GRF(3, RK_PA0, 4, 0x10268, WRITE_MASK_VAL(11, 10, 2)), /* UART3_TX_M2 */

	RK_MUXROUTE_GRF(3, RK_PA4, 4, 0x10268, WRITE_MASK_VAL(13, 12, 0)), /* UART4_TX_M0 */
	RK_MUXROUTE_GRF(2, RK_PA6, 4, 0x10268, WRITE_MASK_VAL(13, 12, 1)), /* UART4_TX_M1 */
	RK_MUXROUTE_GRF(1, RK_PD5, 3, 0x10268, WRITE_MASK_VAL(13, 12, 2)), /* UART4_TX_M2 */

	RK_MUXROUTE_GRF(3, RK_PA6, 4, 0x10268, WRITE_MASK_VAL(15, 14, 0)), /* UART5_TX_M0 */
	RK_MUXROUTE_GRF(2, RK_PB0, 4, 0x10268, WRITE_MASK_VAL(15, 14, 1)), /* UART5_TX_M1 */
	RK_MUXROUTE_GRF(2, RK_PA0, 3, 0x10268, WRITE_MASK_VAL(15, 14, 2)), /* UART5_TX_M2 */

	RK_MUXROUTE_PMU(0, RK_PB6, 3, 0x0114, WRITE_MASK_VAL(0, 0, 0)), /* PWM0_M0 */
	RK_MUXROUTE_PMU(2, RK_PB3, 5, 0x0114, WRITE_MASK_VAL(0, 0, 1)), /* PWM0_M1 */

	RK_MUXROUTE_PMU(0, RK_PB7, 3, 0x0114, WRITE_MASK_VAL(2, 2, 0)), /* PWM1_M0 */
	RK_MUXROUTE_PMU(2, RK_PB2, 5, 0x0114, WRITE_MASK_VAL(2, 2, 1)), /* PWM1_M1 */

	RK_MUXROUTE_PMU(0, RK_PC0, 3, 0x0114, WRITE_MASK_VAL(4, 4, 0)), /* PWM2_M0 */
	RK_MUXROUTE_PMU(2, RK_PB1, 5, 0x0114, WRITE_MASK_VAL(4, 4, 1)), /* PWM2_M1 */

	RK_MUXROUTE_PMU(0, RK_PC1, 3, 0x0114, WRITE_MASK_VAL(6, 6, 0)), /* PWM3_IR_M0 */
	RK_MUXROUTE_PMU(2, RK_PB0, 5, 0x0114, WRITE_MASK_VAL(6, 6, 1)), /* PWM3_IR_M1 */

	RK_MUXROUTE_PMU(0, RK_PC2, 3, 0x0114, WRITE_MASK_VAL(8, 8, 0)), /* PWM4_M0 */
	RK_MUXROUTE_PMU(2, RK_PA7, 5, 0x0114, WRITE_MASK_VAL(8, 8, 1)), /* PWM4_M1 */

	RK_MUXROUTE_PMU(0, RK_PC3, 3, 0x0114, WRITE_MASK_VAL(10, 10, 0)), /* PWM5_M0 */
	RK_MUXROUTE_PMU(2, RK_PA6, 5, 0x0114, WRITE_MASK_VAL(10, 10, 1)), /* PWM5_M1 */

	RK_MUXROUTE_PMU(0, RK_PB2, 3, 0x0114, WRITE_MASK_VAL(12, 12, 0)), /* PWM6_M0 */
	RK_MUXROUTE_PMU(2, RK_PD4, 5, 0x0114, WRITE_MASK_VAL(12, 12, 1)), /* PWM6_M1 */

	RK_MUXROUTE_PMU(0, RK_PB1, 3, 0x0114, WRITE_MASK_VAL(14, 14, 0)), /* PWM7_IR_M0 */
	RK_MUXROUTE_PMU(3, RK_PA0, 5, 0x0114, WRITE_MASK_VAL(14, 14, 1)), /* PWM7_IR_M1 */

	RK_MUXROUTE_PMU(0, RK_PB0, 1, 0x0118, WRITE_MASK_VAL(1, 0, 0)), /* SPI0_CLK_M0 */
	RK_MUXROUTE_PMU(2, RK_PA1, 1, 0x0118, WRITE_MASK_VAL(1, 0, 1)), /* SPI0_CLK_M1 */
	RK_MUXROUTE_PMU(2, RK_PB2, 6, 0x0118, WRITE_MASK_VAL(1, 0, 2)), /* SPI0_CLK_M2 */

	RK_MUXROUTE_PMU(0, RK_PB6, 2, 0x0118, WRITE_MASK_VAL(2, 2, 0)), /* UART1_TX_M0 */
	RK_MUXROUTE_PMU(1, RK_PD0, 5, 0x0118, WRITE_MASK_VAL(2, 2, 1)), /* UART1_TX_M1 */
};

static struct rockchip_mux_route_data rk3128_mux_route_data[] = {
	RK_MUXROUTE_SAME(1, RK_PB2, 1, 0x144, BIT(16 + 3) | BIT(16 + 4)), /* spi-0 */
	RK_MUXROUTE_SAME(1, RK_PD3, 3, 0x144, BIT(16 + 3) | BIT(16 + 4) | BIT(3)), /* spi-1 */
	RK_MUXROUTE_SAME(0, RK_PB5, 2, 0x144, BIT(16 + 3) | BIT(16 + 4) | BIT(4)), /* spi-2 */
	RK_MUXROUTE_SAME(1, RK_PA5, 1, 0x144, BIT(16 + 5)), /* i2s-0 */
	RK_MUXROUTE_SAME(0, RK_PB6, 1, 0x144, BIT(16 + 5) | BIT(5)), /* i2s-1 */
	RK_MUXROUTE_SAME(1, RK_PC6, 2, 0x144, BIT(16 + 6)), /* emmc-0 */
	RK_MUXROUTE_SAME(2, RK_PA4, 2, 0x144, BIT(16 + 6) | BIT(6)), /* emmc-1 */
};

static struct rockchip_mux_route_data rk3188_mux_route_data[] = {
	RK_MUXROUTE_SAME(0, RK_PD0, 1, 0xa0, BIT(16 + 11)), /* non-iomuxed emmc/flash pins on flash-dqs */
	RK_MUXROUTE_SAME(0, RK_PD0, 2, 0xa0, BIT(16 + 11) | BIT(11)), /* non-iomuxed emmc/flash pins on emmc-clk */
};

static struct rockchip_mux_route_data rk3228_mux_route_data[] = {
	RK_MUXROUTE_SAME(0, RK_PD2, 1, 0x50, BIT(16)), /* pwm0-0 */
	RK_MUXROUTE_SAME(3, RK_PC5, 1, 0x50, BIT(16) | BIT(0)), /* pwm0-1 */
	RK_MUXROUTE_SAME(0, RK_PD3, 1, 0x50, BIT(16 + 1)), /* pwm1-0 */
	RK_MUXROUTE_SAME(0, RK_PD6, 2, 0x50, BIT(16 + 1) | BIT(1)), /* pwm1-1 */
	RK_MUXROUTE_SAME(0, RK_PD4, 1, 0x50, BIT(16 + 2)), /* pwm2-0 */
	RK_MUXROUTE_SAME(1, RK_PB4, 2, 0x50, BIT(16 + 2) | BIT(2)), /* pwm2-1 */
	RK_MUXROUTE_SAME(3, RK_PD2, 1, 0x50, BIT(16 + 3)), /* pwm3-0 */
	RK_MUXROUTE_SAME(1, RK_PB3, 2, 0x50, BIT(16 + 3) | BIT(3)), /* pwm3-1 */
	RK_MUXROUTE_SAME(1, RK_PA1, 1, 0x50, BIT(16 + 4)), /* sdio-0_d0 */
	RK_MUXROUTE_SAME(3, RK_PA2, 1, 0x50, BIT(16 + 4) | BIT(4)), /* sdio-1_d0 */
	RK_MUXROUTE_SAME(0, RK_PB5, 2, 0x50, BIT(16 + 5)), /* spi-0_rx */
	RK_MUXROUTE_SAME(2, RK_PA0, 2, 0x50, BIT(16 + 5) | BIT(5)), /* spi-1_rx */
	RK_MUXROUTE_SAME(1, RK_PC6, 2, 0x50, BIT(16 + 7)), /* emmc-0_cmd */
	RK_MUXROUTE_SAME(2, RK_PA4, 2, 0x50, BIT(16 + 7) | BIT(7)), /* emmc-1_cmd */
	RK_MUXROUTE_SAME(1, RK_PC3, 2, 0x50, BIT(16 + 8)), /* uart2-0_rx */
	RK_MUXROUTE_SAME(1, RK_PB2, 2, 0x50, BIT(16 + 8) | BIT(8)), /* uart2-1_rx */
	RK_MUXROUTE_SAME(1, RK_PB2, 1, 0x50, BIT(16 + 11)), /* uart1-0_rx */
	RK_MUXROUTE_SAME(3, RK_PB5, 1, 0x50, BIT(16 + 11) | BIT(11)), /* uart1-1_rx */
};

static struct rockchip_mux_route_data rk3288_mux_route_data[] = {
	RK_MUXROUTE_SAME(7, RK_PC0, 2, 0x264, BIT(16 + 12) | BIT(12)), /* edphdmi_cecinoutt1 */
	RK_MUXROUTE_SAME(7, RK_PC7, 4, 0x264, BIT(16 + 12)), /* edphdmi_cecinout */
};

static struct rockchip_mux_route_data rk3308_mux_route_data[] = {
	RK_MUXROUTE_SAME(0, RK_PC3, 1, 0x314, BIT(16 + 0) | BIT(0)), /* rtc_clk */
	RK_MUXROUTE_SAME(1, RK_PC6, 2, 0x314, BIT(16 + 2) | BIT(16 + 3)), /* uart2_rxm0 */
	RK_MUXROUTE_SAME(4, RK_PD2, 2, 0x314, BIT(16 + 2) | BIT(16 + 3) | BIT(2)), /* uart2_rxm1 */
	RK_MUXROUTE_SAME(0, RK_PB7, 2, 0x608, BIT(16 + 8) | BIT(16 + 9)), /* i2c3_sdam0 */
	RK_MUXROUTE_SAME(3, RK_PB4, 2, 0x608, BIT(16 + 8) | BIT(16 + 9) | BIT(8)), /* i2c3_sdam1 */
	RK_MUXROUTE_SAME(2, RK_PA0, 3, 0x608, BIT(16 + 8) | BIT(16 + 9) | BIT(9)), /* i2c3_sdam2 */
	RK_MUXROUTE_SAME(1, RK_PA3, 2, 0x308, BIT(16 + 3)), /* i2s-8ch-1-sclktxm0 */
	RK_MUXROUTE_SAME(1, RK_PA4, 2, 0x308, BIT(16 + 3)), /* i2s-8ch-1-sclkrxm0 */
	RK_MUXROUTE_SAME(1, RK_PB5, 2, 0x308, BIT(16 + 3) | BIT(3)), /* i2s-8ch-1-sclktxm1 */
	RK_MUXROUTE_SAME(1, RK_PB6, 2, 0x308, BIT(16 + 3) | BIT(3)), /* i2s-8ch-1-sclkrxm1 */
	RK_MUXROUTE_SAME(1, RK_PA4, 3, 0x308, BIT(16 + 12) | BIT(16 + 13)), /* pdm-clkm0 */
	RK_MUXROUTE_SAME(1, RK_PB6, 4, 0x308, BIT(16 + 12) | BIT(16 + 13) | BIT(12)), /* pdm-clkm1 */
	RK_MUXROUTE_SAME(2, RK_PA6, 2, 0x308, BIT(16 + 12) | BIT(16 + 13) | BIT(13)), /* pdm-clkm2 */
	RK_MUXROUTE_SAME(2, RK_PA4, 3, 0x600, BIT(16 + 2) | BIT(2)), /* pdm-clkm-m2 */
	RK_MUXROUTE_SAME(3, RK_PB2, 3, 0x314, BIT(16 + 9)), /* spi1_miso */
	RK_MUXROUTE_SAME(2, RK_PA4, 2, 0x314, BIT(16 + 9) | BIT(9)), /* spi1_miso_m1 */
	RK_MUXROUTE_SAME(0, RK_PB3, 3, 0x314, BIT(16 + 10) | BIT(16 + 11)), /* owire_m0 */
	RK_MUXROUTE_SAME(1, RK_PC6, 7, 0x314, BIT(16 + 10) | BIT(16 + 11) | BIT(10)), /* owire_m1 */
	RK_MUXROUTE_SAME(2, RK_PA2, 5, 0x314, BIT(16 + 10) | BIT(16 + 11) | BIT(11)), /* owire_m2 */
	RK_MUXROUTE_SAME(0, RK_PB3, 2, 0x314, BIT(16 + 12) | BIT(16 + 13)), /* can_rxd_m0 */
	RK_MUXROUTE_SAME(1, RK_PC6, 5, 0x314, BIT(16 + 12) | BIT(16 + 13) | BIT(12)), /* can_rxd_m1 */
	RK_MUXROUTE_SAME(2, RK_PA2, 4, 0x314, BIT(16 + 12) | BIT(16 + 13) | BIT(13)), /* can_rxd_m2 */
	RK_MUXROUTE_SAME(1, RK_PC4, 3, 0x314, BIT(16 + 14)), /* mac_rxd0_m0 */
	RK_MUXROUTE_SAME(4, RK_PA2, 2, 0x314, BIT(16 + 14) | BIT(14)), /* mac_rxd0_m1 */
	RK_MUXROUTE_SAME(3, RK_PB4, 4, 0x314, BIT(16 + 15)), /* uart3_rx */
	RK_MUXROUTE_SAME(0, RK_PC1, 3, 0x314, BIT(16 + 15) | BIT(15)), /* uart3_rx_m1 */
};

static struct rockchip_mux_route_data rk3328_mux_route_data[] = {
	RK_MUXROUTE_SAME(1, RK_PA1, 2, 0x50, BIT(16) | BIT(16 + 1)), /* uart2dbg_rxm0 */
	RK_MUXROUTE_SAME(2, RK_PA1, 1, 0x50, BIT(16) | BIT(16 + 1) | BIT(0)), /* uart2dbg_rxm1 */
	RK_MUXROUTE_SAME(1, RK_PB3, 2, 0x50, BIT(16 + 2) | BIT(2)), /* gmac-m1_rxd0 */
	RK_MUXROUTE_SAME(1, RK_PB6, 2, 0x50, BIT(16 + 10) | BIT(10)), /* gmac-m1-optimized_rxd3 */
	RK_MUXROUTE_SAME(2, RK_PC3, 2, 0x50, BIT(16 + 3)), /* pdm_sdi0m0 */
	RK_MUXROUTE_SAME(1, RK_PC7, 3, 0x50, BIT(16 + 3) | BIT(3)), /* pdm_sdi0m1 */
	RK_MUXROUTE_SAME(3, RK_PA2, 4, 0x50, BIT(16 + 4) | BIT(16 + 5) | BIT(5)), /* spi_rxdm2 */
	RK_MUXROUTE_SAME(1, RK_PD0, 1, 0x50, BIT(16 + 6)), /* i2s2_sdim0 */
	RK_MUXROUTE_SAME(3, RK_PA2, 6, 0x50, BIT(16 + 6) | BIT(6)), /* i2s2_sdim1 */
	RK_MUXROUTE_SAME(2, RK_PC6, 3, 0x50, BIT(16 + 7) | BIT(7)), /* card_iom1 */
	RK_MUXROUTE_SAME(2, RK_PC0, 3, 0x50, BIT(16 + 8) | BIT(8)), /* tsp_d5m1 */
	RK_MUXROUTE_SAME(2, RK_PC0, 4, 0x50, BIT(16 + 9) | BIT(9)), /* cif_data5m1 */
};

static struct rockchip_mux_route_data rk3399_mux_route_data[] = {
	RK_MUXROUTE_SAME(4, RK_PB0, 2, 0xe21c, BIT(16 + 10) | BIT(16 + 11)), /* uart2dbga_rx */
	RK_MUXROUTE_SAME(4, RK_PC0, 2, 0xe21c, BIT(16 + 10) | BIT(16 + 11) | BIT(10)), /* uart2dbgb_rx */
	RK_MUXROUTE_SAME(4, RK_PC3, 1, 0xe21c, BIT(16 + 10) | BIT(16 + 11) | BIT(11)), /* uart2dbgc_rx */
	RK_MUXROUTE_SAME(2, RK_PD2, 2, 0xe21c, BIT(16 + 14)), /* pcie_clkreqn */
	RK_MUXROUTE_SAME(4, RK_PD0, 1, 0xe21c, BIT(16 + 14) | BIT(14)), /* pcie_clkreqnb */
};

static struct rockchip_mux_route_data rk3568_mux_route_data[] = {
	RK_MUXROUTE_PMU(0, RK_PB7, 1, 0x0110, WRITE_MASK_VAL(1, 0, 0)), /* PWM0 IO mux M0 */
	RK_MUXROUTE_PMU(0, RK_PC7, 2, 0x0110, WRITE_MASK_VAL(1, 0, 1)), /* PWM0 IO mux M1 */
	RK_MUXROUTE_PMU(0, RK_PC0, 1, 0x0110, WRITE_MASK_VAL(3, 2, 0)), /* PWM1 IO mux M0 */
	RK_MUXROUTE_PMU(0, RK_PB5, 4, 0x0110, WRITE_MASK_VAL(3, 2, 1)), /* PWM1 IO mux M1 */
	RK_MUXROUTE_PMU(0, RK_PC1, 1, 0x0110, WRITE_MASK_VAL(5, 4, 0)), /* PWM2 IO mux M0 */
	RK_MUXROUTE_PMU(0, RK_PB6, 4, 0x0110, WRITE_MASK_VAL(5, 4, 1)), /* PWM2 IO mux M1 */
	RK_MUXROUTE_GRF(0, RK_PB3, 2, 0x0300, WRITE_MASK_VAL(0, 0, 0)), /* CAN0 IO mux M0 */
	RK_MUXROUTE_GRF(2, RK_PA1, 4, 0x0300, WRITE_MASK_VAL(0, 0, 1)), /* CAN0 IO mux M1 */
	RK_MUXROUTE_GRF(1, RK_PA1, 3, 0x0300, WRITE_MASK_VAL(2, 2, 0)), /* CAN1 IO mux M0 */
	RK_MUXROUTE_GRF(4, RK_PC3, 3, 0x0300, WRITE_MASK_VAL(2, 2, 1)), /* CAN1 IO mux M1 */
	RK_MUXROUTE_GRF(4, RK_PB5, 3, 0x0300, WRITE_MASK_VAL(4, 4, 0)), /* CAN2 IO mux M0 */
	RK_MUXROUTE_GRF(2, RK_PB2, 4, 0x0300, WRITE_MASK_VAL(4, 4, 1)), /* CAN2 IO mux M1 */
	RK_MUXROUTE_GRF(4, RK_PC4, 1, 0x0300, WRITE_MASK_VAL(6, 6, 0)), /* HPDIN IO mux M0 */
	RK_MUXROUTE_GRF(0, RK_PC2, 2, 0x0300, WRITE_MASK_VAL(6, 6, 1)), /* HPDIN IO mux M1 */
	RK_MUXROUTE_GRF(3, RK_PB1, 3, 0x0300, WRITE_MASK_VAL(8, 8, 0)), /* GMAC1 IO mux M0 */
	RK_MUXROUTE_GRF(4, RK_PA7, 3, 0x0300, WRITE_MASK_VAL(8, 8, 1)), /* GMAC1 IO mux M1 */
	RK_MUXROUTE_GRF(4, RK_PD1, 1, 0x0300, WRITE_MASK_VAL(10, 10, 0)), /* HDMITX IO mux M0 */
	RK_MUXROUTE_GRF(0, RK_PC7, 1, 0x0300, WRITE_MASK_VAL(10, 10, 1)), /* HDMITX IO mux M1 */
	RK_MUXROUTE_GRF(0, RK_PB6, 1, 0x0300, WRITE_MASK_VAL(14, 14, 0)), /* I2C2 IO mux M0 */
	RK_MUXROUTE_GRF(4, RK_PB4, 1, 0x0300, WRITE_MASK_VAL(14, 14, 1)), /* I2C2 IO mux M1 */
	RK_MUXROUTE_GRF(1, RK_PA0, 1, 0x0304, WRITE_MASK_VAL(0, 0, 0)), /* I2C3 IO mux M0 */
	RK_MUXROUTE_GRF(3, RK_PB6, 4, 0x0304, WRITE_MASK_VAL(0, 0, 1)), /* I2C3 IO mux M1 */
	RK_MUXROUTE_GRF(4, RK_PB2, 1, 0x0304, WRITE_MASK_VAL(2, 2, 0)), /* I2C4 IO mux M0 */
	RK_MUXROUTE_GRF(2, RK_PB1, 2, 0x0304, WRITE_MASK_VAL(2, 2, 1)), /* I2C4 IO mux M1 */
	RK_MUXROUTE_GRF(3, RK_PB4, 4, 0x0304, WRITE_MASK_VAL(4, 4, 0)), /* I2C5 IO mux M0 */
	RK_MUXROUTE_GRF(4, RK_PD0, 2, 0x0304, WRITE_MASK_VAL(4, 4, 1)), /* I2C5 IO mux M1 */
	RK_MUXROUTE_GRF(3, RK_PB1, 5, 0x0304, WRITE_MASK_VAL(14, 14, 0)), /* PWM8 IO mux M0 */
	RK_MUXROUTE_GRF(1, RK_PD5, 4, 0x0304, WRITE_MASK_VAL(14, 14, 1)), /* PWM8 IO mux M1 */
	RK_MUXROUTE_GRF(3, RK_PB2, 5, 0x0308, WRITE_MASK_VAL(0, 0, 0)), /* PWM9 IO mux M0 */
	RK_MUXROUTE_GRF(1, RK_PD6, 4, 0x0308, WRITE_MASK_VAL(0, 0, 1)), /* PWM9 IO mux M1 */
	RK_MUXROUTE_GRF(3, RK_PB5, 5, 0x0308, WRITE_MASK_VAL(2, 2, 0)), /* PWM10 IO mux M0 */
	RK_MUXROUTE_GRF(2, RK_PA1, 2, 0x0308, WRITE_MASK_VAL(2, 2, 1)), /* PWM10 IO mux M1 */
	RK_MUXROUTE_GRF(3, RK_PB6, 5, 0x0308, WRITE_MASK_VAL(4, 4, 0)), /* PWM11 IO mux M0 */
	RK_MUXROUTE_GRF(4, RK_PC0, 3, 0x0308, WRITE_MASK_VAL(4, 4, 1)), /* PWM11 IO mux M1 */
	RK_MUXROUTE_GRF(3, RK_PB7, 2, 0x0308, WRITE_MASK_VAL(6, 6, 0)), /* PWM12 IO mux M0 */
	RK_MUXROUTE_GRF(4, RK_PC5, 1, 0x0308, WRITE_MASK_VAL(6, 6, 1)), /* PWM12 IO mux M1 */
	RK_MUXROUTE_GRF(3, RK_PC0, 2, 0x0308, WRITE_MASK_VAL(8, 8, 0)), /* PWM13 IO mux M0 */
	RK_MUXROUTE_GRF(4, RK_PC6, 1, 0x0308, WRITE_MASK_VAL(8, 8, 1)), /* PWM13 IO mux M1 */
	RK_MUXROUTE_GRF(3, RK_PC4, 1, 0x0308, WRITE_MASK_VAL(10, 10, 0)), /* PWM14 IO mux M0 */
	RK_MUXROUTE_GRF(4, RK_PC2, 1, 0x0308, WRITE_MASK_VAL(10, 10, 1)), /* PWM14 IO mux M1 */
	RK_MUXROUTE_GRF(3, RK_PC5, 1, 0x0308, WRITE_MASK_VAL(12, 12, 0)), /* PWM15 IO mux M0 */
	RK_MUXROUTE_GRF(4, RK_PC3, 1, 0x0308, WRITE_MASK_VAL(12, 12, 1)), /* PWM15 IO mux M1 */
	RK_MUXROUTE_GRF(3, RK_PD2, 3, 0x0308, WRITE_MASK_VAL(14, 14, 0)), /* SDMMC2 IO mux M0 */
	RK_MUXROUTE_GRF(3, RK_PA5, 5, 0x0308, WRITE_MASK_VAL(14, 14, 1)), /* SDMMC2 IO mux M1 */
	RK_MUXROUTE_GRF(0, RK_PB5, 2, 0x030c, WRITE_MASK_VAL(0, 0, 0)), /* SPI0 IO mux M0 */
	RK_MUXROUTE_GRF(2, RK_PD3, 3, 0x030c, WRITE_MASK_VAL(0, 0, 1)), /* SPI0 IO mux M1 */
	RK_MUXROUTE_GRF(2, RK_PB5, 3, 0x030c, WRITE_MASK_VAL(2, 2, 0)), /* SPI1 IO mux M0 */
	RK_MUXROUTE_GRF(3, RK_PC3, 3, 0x030c, WRITE_MASK_VAL(2, 2, 1)), /* SPI1 IO mux M1 */
	RK_MUXROUTE_GRF(2, RK_PC1, 4, 0x030c, WRITE_MASK_VAL(4, 4, 0)), /* SPI2 IO mux M0 */
	RK_MUXROUTE_GRF(3, RK_PA0, 3, 0x030c, WRITE_MASK_VAL(4, 4, 1)), /* SPI2 IO mux M1 */
	RK_MUXROUTE_GRF(4, RK_PB3, 4, 0x030c, WRITE_MASK_VAL(6, 6, 0)), /* SPI3 IO mux M0 */
	RK_MUXROUTE_GRF(4, RK_PC2, 2, 0x030c, WRITE_MASK_VAL(6, 6, 1)), /* SPI3 IO mux M1 */
	RK_MUXROUTE_GRF(2, RK_PB4, 2, 0x030c, WRITE_MASK_VAL(8, 8, 0)), /* UART1 IO mux M0 */
	RK_MUXROUTE_GRF(3, RK_PD6, 4, 0x030c, WRITE_MASK_VAL(8, 8, 1)), /* UART1 IO mux M1 */
	RK_MUXROUTE_GRF(0, RK_PD1, 1, 0x030c, WRITE_MASK_VAL(10, 10, 0)), /* UART2 IO mux M0 */
	RK_MUXROUTE_GRF(1, RK_PD5, 2, 0x030c, WRITE_MASK_VAL(10, 10, 1)), /* UART2 IO mux M1 */
	RK_MUXROUTE_GRF(1, RK_PA1, 2, 0x030c, WRITE_MASK_VAL(12, 12, 0)), /* UART3 IO mux M0 */
	RK_MUXROUTE_GRF(3, RK_PB7, 4, 0x030c, WRITE_MASK_VAL(12, 12, 1)), /* UART3 IO mux M1 */
	RK_MUXROUTE_GRF(1, RK_PA6, 2, 0x030c, WRITE_MASK_VAL(14, 14, 0)), /* UART4 IO mux M0 */
	RK_MUXROUTE_GRF(3, RK_PB2, 4, 0x030c, WRITE_MASK_VAL(14, 14, 1)), /* UART4 IO mux M1 */
	RK_MUXROUTE_GRF(2, RK_PA2, 3, 0x0310, WRITE_MASK_VAL(0, 0, 0)), /* UART5 IO mux M0 */
	RK_MUXROUTE_GRF(3, RK_PC2, 4, 0x0310, WRITE_MASK_VAL(0, 0, 1)), /* UART5 IO mux M1 */
	RK_MUXROUTE_GRF(2, RK_PA4, 3, 0x0310, WRITE_MASK_VAL(2, 2, 0)), /* UART6 IO mux M0 */
	RK_MUXROUTE_GRF(1, RK_PD5, 3, 0x0310, WRITE_MASK_VAL(2, 2, 1)), /* UART6 IO mux M1 */
	RK_MUXROUTE_GRF(2, RK_PA6, 3, 0x0310, WRITE_MASK_VAL(5, 4, 0)), /* UART7 IO mux M0 */
	RK_MUXROUTE_GRF(3, RK_PC4, 4, 0x0310, WRITE_MASK_VAL(5, 4, 1)), /* UART7 IO mux M1 */
	RK_MUXROUTE_GRF(4, RK_PA2, 4, 0x0310, WRITE_MASK_VAL(5, 4, 2)), /* UART7 IO mux M2 */
	RK_MUXROUTE_GRF(2, RK_PC5, 3, 0x0310, WRITE_MASK_VAL(6, 6, 0)), /* UART8 IO mux M0 */
	RK_MUXROUTE_GRF(2, RK_PD7, 4, 0x0310, WRITE_MASK_VAL(6, 6, 1)), /* UART8 IO mux M1 */
	RK_MUXROUTE_GRF(2, RK_PB0, 3, 0x0310, WRITE_MASK_VAL(9, 8, 0)), /* UART9 IO mux M0 */
	RK_MUXROUTE_GRF(4, RK_PC5, 4, 0x0310, WRITE_MASK_VAL(9, 8, 1)), /* UART9 IO mux M1 */
	RK_MUXROUTE_GRF(4, RK_PA4, 4, 0x0310, WRITE_MASK_VAL(9, 8, 2)), /* UART9 IO mux M2 */
	RK_MUXROUTE_GRF(1, RK_PA2, 1, 0x0310, WRITE_MASK_VAL(11, 10, 0)), /* I2S1 IO mux M0 */
	RK_MUXROUTE_GRF(3, RK_PC6, 4, 0x0310, WRITE_MASK_VAL(11, 10, 1)), /* I2S1 IO mux M1 */
	RK_MUXROUTE_GRF(2, RK_PD0, 5, 0x0310, WRITE_MASK_VAL(11, 10, 2)), /* I2S1 IO mux M2 */
	RK_MUXROUTE_GRF(2, RK_PC1, 1, 0x0310, WRITE_MASK_VAL(12, 12, 0)), /* I2S2 IO mux M0 */
	RK_MUXROUTE_GRF(4, RK_PB6, 5, 0x0310, WRITE_MASK_VAL(12, 12, 1)), /* I2S2 IO mux M1 */
	RK_MUXROUTE_GRF(3, RK_PA2, 4, 0x0310, WRITE_MASK_VAL(14, 14, 0)), /* I2S3 IO mux M0 */
	RK_MUXROUTE_GRF(4, RK_PC2, 5, 0x0310, WRITE_MASK_VAL(14, 14, 1)), /* I2S3 IO mux M1 */
	RK_MUXROUTE_GRF(1, RK_PA4, 3, 0x0314, WRITE_MASK_VAL(1, 0, 0)), /* PDM IO mux M0 */
	RK_MUXROUTE_GRF(1, RK_PA6, 3, 0x0314, WRITE_MASK_VAL(1, 0, 0)), /* PDM IO mux M0 */
	RK_MUXROUTE_GRF(3, RK_PD6, 5, 0x0314, WRITE_MASK_VAL(1, 0, 1)), /* PDM IO mux M1 */
	RK_MUXROUTE_GRF(4, RK_PA0, 4, 0x0314, WRITE_MASK_VAL(1, 0, 1)), /* PDM IO mux M1 */
	RK_MUXROUTE_GRF(3, RK_PC4, 5, 0x0314, WRITE_MASK_VAL(1, 0, 2)), /* PDM IO mux M2 */
	RK_MUXROUTE_GRF(0, RK_PA5, 3, 0x0314, WRITE_MASK_VAL(3, 2, 0)), /* PCIE20 IO mux M0 */
	RK_MUXROUTE_GRF(2, RK_PD0, 4, 0x0314, WRITE_MASK_VAL(3, 2, 1)), /* PCIE20 IO mux M1 */
	RK_MUXROUTE_GRF(1, RK_PB0, 4, 0x0314, WRITE_MASK_VAL(3, 2, 2)), /* PCIE20 IO mux M2 */
	RK_MUXROUTE_GRF(0, RK_PA4, 3, 0x0314, WRITE_MASK_VAL(5, 4, 0)), /* PCIE30X1 IO mux M0 */
	RK_MUXROUTE_GRF(2, RK_PD2, 4, 0x0314, WRITE_MASK_VAL(5, 4, 1)), /* PCIE30X1 IO mux M1 */
	RK_MUXROUTE_GRF(1, RK_PA5, 4, 0x0314, WRITE_MASK_VAL(5, 4, 2)), /* PCIE30X1 IO mux M2 */
	RK_MUXROUTE_GRF(0, RK_PA6, 2, 0x0314, WRITE_MASK_VAL(7, 6, 0)), /* PCIE30X2 IO mux M0 */
	RK_MUXROUTE_GRF(2, RK_PD4, 4, 0x0314, WRITE_MASK_VAL(7, 6, 1)), /* PCIE30X2 IO mux M1 */
	RK_MUXROUTE_GRF(4, RK_PC2, 4, 0x0314, WRITE_MASK_VAL(7, 6, 2)), /* PCIE30X2 IO mux M2 */
};

static bool rockchip_get_mux_route(struct rockchip_pin_bank *bank, int pin,
				   int mux, u32 *loc, u32 *reg, u32 *value)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct rockchip_mux_route_data *data = NULL;
	int i;

	for (i = 0; i < ctrl->niomux_routes; i++) {
		data = &ctrl->iomux_routes[i];
		if ((data->bank_num == bank->bank_num) &&
		    (data->pin == pin) && (data->func == mux))
			break;
	}

	if (i >= ctrl->niomux_routes)
		return false;

	*loc = data->route_location;
	*reg = data->route_offset;
	*value = data->route_val;

	return true;
}

static int rockchip_verify_mux(struct rockchip_pin_bank *bank,
			       int pin, int mux)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct device *dev = info->dev;
	int iomux_num = (pin / 8);

	if (iomux_num > 3)
		return -EINVAL;

	if (bank->iomux[iomux_num].type & IOMUX_UNROUTED) {
		dev_err(dev, "pin %d is unrouted\n", pin);
		return -EINVAL;
	}

	if (bank->iomux[iomux_num].type & IOMUX_GPIO_ONLY) {
		if (mux != RK_FUNC_GPIO) {
			dev_err(dev, "pin %d only supports a gpio mux\n", pin);
			return -ENOTSUPP;
		}
	}

	return 0;
}

/*
 * Set a new mux function for a pin.
 *
 * The register is divided into the upper and lower 16 bit. When changing
 * a value, the previous register value is not read and changed. Instead
 * it seems the changed bits are marked in the upper 16 bit, while the
 * changed value gets set in the same offset in the lower 16 bit.
 * All pin settings seem to be 2 bit wide in both the upper and lower
 * parts.
 * @bank: pin bank to change
 * @pin: pin to change
 * @mux: new mux function to set
 */
static int rockchip_set_mux(struct rockchip_pin_bank *bank, int pin, int mux)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct device *dev = info->dev;
	int iomux_num = (pin / 8);
	struct regmap *regmap;
	int reg, ret, mask, mux_type;
	u8 bit;
	u32 data, rmask, route_location, route_reg, route_val;

	ret = rockchip_verify_mux(bank, pin, mux);
	if (ret < 0)
		return ret;

	if (bank->iomux[iomux_num].type & IOMUX_GPIO_ONLY)
		return 0;

	dev_dbg(dev, "setting mux of GPIO%d-%d to %d\n", bank->bank_num, pin, mux);

	if (bank->iomux[iomux_num].type & IOMUX_SOURCE_PMU)
		regmap = info->regmap_pmu;
	else if (bank->iomux[iomux_num].type & IOMUX_L_SOURCE_PMU)
		regmap = (pin % 8 < 4) ? info->regmap_pmu : info->regmap_base;
	else
		regmap = info->regmap_base;

	/* get basic quadrupel of mux registers and the correct reg inside */
	mux_type = bank->iomux[iomux_num].type;
	reg = bank->iomux[iomux_num].offset;
	if (mux_type & IOMUX_WIDTH_4BIT) {
		if ((pin % 8) >= 4)
			reg += 0x4;
		bit = (pin % 4) * 4;
		mask = 0xf;
	} else if (mux_type & IOMUX_WIDTH_3BIT) {
		if ((pin % 8) >= 5)
			reg += 0x4;
		bit = (pin % 8 % 5) * 3;
		mask = 0x7;
	} else {
		bit = (pin % 8) * 2;
		mask = 0x3;
	}

	if (bank->recalced_mask & BIT(pin))
		rockchip_get_recalced_mux(bank, pin, &reg, &bit, &mask);

	if (ctrl->type == RK3588) {
		if (bank->bank_num == 0) {
			if ((pin >= RK_PB4) && (pin <= RK_PD7)) {
				if (mux < 8) {
					reg += 0x4000 - 0xC; /* PMU2_IOC_BASE */
					data = (mask << (bit + 16));
					rmask = data | (data >> 16);
					data |= (mux & mask) << bit;
					ret = regmap_update_bits(regmap, reg, rmask, data);
				} else {
					u32 reg0 = 0;

					reg0 = reg + 0x4000 - 0xC; /* PMU2_IOC_BASE */
					data = (mask << (bit + 16));
					rmask = data | (data >> 16);
					data |= 8 << bit;
					ret = regmap_update_bits(regmap, reg0, rmask, data);

					reg0 = reg + 0x8000; /* BUS_IOC_BASE */
					data = (mask << (bit + 16));
					rmask = data | (data >> 16);
					data |= mux << bit;
					regmap = info->regmap_base;
					ret |= regmap_update_bits(regmap, reg0, rmask, data);
				}
			} else {
				data = (mask << (bit + 16));
				rmask = data | (data >> 16);
				data |= (mux & mask) << bit;
				ret = regmap_update_bits(regmap, reg, rmask, data);
			}
			return ret;
		} else if (bank->bank_num > 0) {
			reg += 0x8000; /* BUS_IOC_BASE */
		}
	}

	if (mux > mask)
		return -EINVAL;

	if (bank->route_mask & BIT(pin)) {
		if (rockchip_get_mux_route(bank, pin, mux, &route_location,
					   &route_reg, &route_val)) {
			struct regmap *route_regmap = regmap;

			/* handle special locations */
			switch (route_location) {
			case ROCKCHIP_ROUTE_PMU:
				route_regmap = info->regmap_pmu;
				break;
			case ROCKCHIP_ROUTE_GRF:
				route_regmap = info->regmap_base;
				break;
			}

			ret = regmap_write(route_regmap, route_reg, route_val);
			if (ret)
				return ret;
		}
	}

	data = (mask << (bit + 16));
	rmask = data | (data >> 16);
	data |= (mux & mask) << bit;
	ret = regmap_update_bits(regmap, reg, rmask, data);

	return ret;
}

#define PX30_PULL_PMU_OFFSET		0x10
#define PX30_PULL_GRF_OFFSET		0x60
#define PX30_PULL_BITS_PER_PIN		2
#define PX30_PULL_PINS_PER_REG		8
#define PX30_PULL_BANK_STRIDE		16

static int px30_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
				      int pin_num, struct regmap **regmap,
				      int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	/* The first 32 pins of the first bank are located in PMU */
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = PX30_PULL_PMU_OFFSET;
	} else {
		*regmap = info->regmap_base;
		*reg = PX30_PULL_GRF_OFFSET;

		/* correct the offset, as we're starting with the 2nd bank */
		*reg -= 0x10;
		*reg += bank->bank_num * PX30_PULL_BANK_STRIDE;
	}

	*reg += ((pin_num / PX30_PULL_PINS_PER_REG) * 4);
	*bit = (pin_num % PX30_PULL_PINS_PER_REG);
	*bit *= PX30_PULL_BITS_PER_PIN;

	return 0;
}

#define PX30_DRV_PMU_OFFSET		0x20
#define PX30_DRV_GRF_OFFSET		0xf0
#define PX30_DRV_BITS_PER_PIN		2
#define PX30_DRV_PINS_PER_REG		8
#define PX30_DRV_BANK_STRIDE		16

static int px30_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				     int pin_num, struct regmap **regmap,
				     int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	/* The first 32 pins of the first bank are located in PMU */
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = PX30_DRV_PMU_OFFSET;
	} else {
		*regmap = info->regmap_base;
		*reg = PX30_DRV_GRF_OFFSET;

		/* correct the offset, as we're starting with the 2nd bank */
		*reg -= 0x10;
		*reg += bank->bank_num * PX30_DRV_BANK_STRIDE;
	}

	*reg += ((pin_num / PX30_DRV_PINS_PER_REG) * 4);
	*bit = (pin_num % PX30_DRV_PINS_PER_REG);
	*bit *= PX30_DRV_BITS_PER_PIN;

	return 0;
}

#define PX30_SCHMITT_PMU_OFFSET			0x38
#define PX30_SCHMITT_GRF_OFFSET			0xc0
#define PX30_SCHMITT_PINS_PER_PMU_REG		16
#define PX30_SCHMITT_BANK_STRIDE		16
#define PX30_SCHMITT_PINS_PER_GRF_REG		8

static int px30_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
					 int pin_num,
					 struct regmap **regmap,
					 int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	int pins_per_reg;

	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = PX30_SCHMITT_PMU_OFFSET;
		pins_per_reg = PX30_SCHMITT_PINS_PER_PMU_REG;
	} else {
		*regmap = info->regmap_base;
		*reg = PX30_SCHMITT_GRF_OFFSET;
		pins_per_reg = PX30_SCHMITT_PINS_PER_GRF_REG;
		*reg += (bank->bank_num  - 1) * PX30_SCHMITT_BANK_STRIDE;
	}

	*reg += ((pin_num / pins_per_reg) * 4);
	*bit = pin_num % pins_per_reg;

	return 0;
}

#define RV1108_PULL_PMU_OFFSET		0x10
#define RV1108_PULL_OFFSET		0x110
#define RV1108_PULL_PINS_PER_REG	8
#define RV1108_PULL_BITS_PER_PIN	2
#define RV1108_PULL_BANK_STRIDE		16

static int rv1108_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	/* The first 24 pins of the first bank are located in PMU */
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RV1108_PULL_PMU_OFFSET;
	} else {
		*reg = RV1108_PULL_OFFSET;
		*regmap = info->regmap_base;
		/* correct the offset, as we're starting with the 2nd bank */
		*reg -= 0x10;
		*reg += bank->bank_num * RV1108_PULL_BANK_STRIDE;
	}

	*reg += ((pin_num / RV1108_PULL_PINS_PER_REG) * 4);
	*bit = (pin_num % RV1108_PULL_PINS_PER_REG);
	*bit *= RV1108_PULL_BITS_PER_PIN;

	return 0;
}

#define RV1108_DRV_PMU_OFFSET		0x20
#define RV1108_DRV_GRF_OFFSET		0x210
#define RV1108_DRV_BITS_PER_PIN		2
#define RV1108_DRV_PINS_PER_REG		8
#define RV1108_DRV_BANK_STRIDE		16

static int rv1108_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	/* The first 24 pins of the first bank are located in PMU */
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RV1108_DRV_PMU_OFFSET;
	} else {
		*regmap = info->regmap_base;
		*reg = RV1108_DRV_GRF_OFFSET;

		/* correct the offset, as we're starting with the 2nd bank */
		*reg -= 0x10;
		*reg += bank->bank_num * RV1108_DRV_BANK_STRIDE;
	}

	*reg += ((pin_num / RV1108_DRV_PINS_PER_REG) * 4);
	*bit = pin_num % RV1108_DRV_PINS_PER_REG;
	*bit *= RV1108_DRV_BITS_PER_PIN;

	return 0;
}

#define RV1108_SCHMITT_PMU_OFFSET		0x30
#define RV1108_SCHMITT_GRF_OFFSET		0x388
#define RV1108_SCHMITT_BANK_STRIDE		8
#define RV1108_SCHMITT_PINS_PER_GRF_REG		16
#define RV1108_SCHMITT_PINS_PER_PMU_REG		8

static int rv1108_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
					   int pin_num,
					   struct regmap **regmap,
					   int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	int pins_per_reg;

	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RV1108_SCHMITT_PMU_OFFSET;
		pins_per_reg = RV1108_SCHMITT_PINS_PER_PMU_REG;
	} else {
		*regmap = info->regmap_base;
		*reg = RV1108_SCHMITT_GRF_OFFSET;
		pins_per_reg = RV1108_SCHMITT_PINS_PER_GRF_REG;
		*reg += (bank->bank_num  - 1) * RV1108_SCHMITT_BANK_STRIDE;
	}
	*reg += ((pin_num / pins_per_reg) * 4);
	*bit = pin_num % pins_per_reg;

	return 0;
}

#define RV1126_PULL_PMU_OFFSET		0x40
#define RV1126_PULL_GRF_GPIO1A0_OFFSET	0x10108
#define RV1126_PULL_PINS_PER_REG	8
#define RV1126_PULL_BITS_PER_PIN	2
#define RV1126_PULL_BANK_STRIDE		16
#define RV1126_GPIO_C4_D7(p)		(p >= 20 && p <= 31) /* GPIO0_C4 ~ GPIO0_D7 */

static int rv1126_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	/* The first 24 pins of the first bank are located in PMU */
	if (bank->bank_num == 0) {
		if (RV1126_GPIO_C4_D7(pin_num)) {
			*regmap = info->regmap_base;
			*reg = RV1126_PULL_GRF_GPIO1A0_OFFSET;
			*reg -= (((31 - pin_num) / RV1126_PULL_PINS_PER_REG + 1) * 4);
			*bit = pin_num % RV1126_PULL_PINS_PER_REG;
			*bit *= RV1126_PULL_BITS_PER_PIN;
			return 0;
		}
		*regmap = info->regmap_pmu;
		*reg = RV1126_PULL_PMU_OFFSET;
	} else {
		*reg = RV1126_PULL_GRF_GPIO1A0_OFFSET;
		*regmap = info->regmap_base;
		*reg += (bank->bank_num - 1) * RV1126_PULL_BANK_STRIDE;
	}

	*reg += ((pin_num / RV1126_PULL_PINS_PER_REG) * 4);
	*bit = (pin_num % RV1126_PULL_PINS_PER_REG);
	*bit *= RV1126_PULL_BITS_PER_PIN;

	return 0;
}

#define RV1126_DRV_PMU_OFFSET		0x20
#define RV1126_DRV_GRF_GPIO1A0_OFFSET	0x10090
#define RV1126_DRV_BITS_PER_PIN		4
#define RV1126_DRV_PINS_PER_REG		4
#define RV1126_DRV_BANK_STRIDE		32

static int rv1126_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	/* The first 24 pins of the first bank are located in PMU */
	if (bank->bank_num == 0) {
		if (RV1126_GPIO_C4_D7(pin_num)) {
			*regmap = info->regmap_base;
			*reg = RV1126_DRV_GRF_GPIO1A0_OFFSET;
			*reg -= (((31 - pin_num) / RV1126_DRV_PINS_PER_REG + 1) * 4);
			*reg -= 0x4;
			*bit = pin_num % RV1126_DRV_PINS_PER_REG;
			*bit *= RV1126_DRV_BITS_PER_PIN;
			return 0;
		}
		*regmap = info->regmap_pmu;
		*reg = RV1126_DRV_PMU_OFFSET;
	} else {
		*regmap = info->regmap_base;
		*reg = RV1126_DRV_GRF_GPIO1A0_OFFSET;
		*reg += (bank->bank_num - 1) * RV1126_DRV_BANK_STRIDE;
	}

	*reg += ((pin_num / RV1126_DRV_PINS_PER_REG) * 4);
	*bit = pin_num % RV1126_DRV_PINS_PER_REG;
	*bit *= RV1126_DRV_BITS_PER_PIN;

	return 0;
}

#define RV1126_SCHMITT_PMU_OFFSET		0x60
#define RV1126_SCHMITT_GRF_GPIO1A0_OFFSET	0x10188
#define RV1126_SCHMITT_BANK_STRIDE		16
#define RV1126_SCHMITT_PINS_PER_GRF_REG		8
#define RV1126_SCHMITT_PINS_PER_PMU_REG		8

static int rv1126_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
					   int pin_num,
					   struct regmap **regmap,
					   int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	int pins_per_reg;

	if (bank->bank_num == 0) {
		if (RV1126_GPIO_C4_D7(pin_num)) {
			*regmap = info->regmap_base;
			*reg = RV1126_SCHMITT_GRF_GPIO1A0_OFFSET;
			*reg -= (((31 - pin_num) / RV1126_SCHMITT_PINS_PER_GRF_REG + 1) * 4);
			*bit = pin_num % RV1126_SCHMITT_PINS_PER_GRF_REG;
			return 0;
		}
		*regmap = info->regmap_pmu;
		*reg = RV1126_SCHMITT_PMU_OFFSET;
		pins_per_reg = RV1126_SCHMITT_PINS_PER_PMU_REG;
	} else {
		*regmap = info->regmap_base;
		*reg = RV1126_SCHMITT_GRF_GPIO1A0_OFFSET;
		pins_per_reg = RV1126_SCHMITT_PINS_PER_GRF_REG;
		*reg += (bank->bank_num - 1) * RV1126_SCHMITT_BANK_STRIDE;
	}
	*reg += ((pin_num / pins_per_reg) * 4);
	*bit = pin_num % pins_per_reg;

	return 0;
}

#define RK3308_SCHMITT_PINS_PER_REG		8
#define RK3308_SCHMITT_BANK_STRIDE		16
#define RK3308_SCHMITT_GRF_OFFSET		0x1a0

static int rk3308_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
				    int pin_num, struct regmap **regmap,
				    int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3308_SCHMITT_GRF_OFFSET;

	*reg += bank->bank_num * RK3308_SCHMITT_BANK_STRIDE;
	*reg += ((pin_num / RK3308_SCHMITT_PINS_PER_REG) * 4);
	*bit = pin_num % RK3308_SCHMITT_PINS_PER_REG;

	return 0;
}

#define RK2928_PULL_OFFSET		0x118
#define RK2928_PULL_PINS_PER_REG	16
#define RK2928_PULL_BANK_STRIDE		8

static int rk2928_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK2928_PULL_OFFSET;
	*reg += bank->bank_num * RK2928_PULL_BANK_STRIDE;
	*reg += (pin_num / RK2928_PULL_PINS_PER_REG) * 4;

	*bit = pin_num % RK2928_PULL_PINS_PER_REG;

	return 0;
};

#define RK3128_PULL_OFFSET	0x118

static int rk3128_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3128_PULL_OFFSET;
	*reg += bank->bank_num * RK2928_PULL_BANK_STRIDE;
	*reg += ((pin_num / RK2928_PULL_PINS_PER_REG) * 4);

	*bit = pin_num % RK2928_PULL_PINS_PER_REG;

	return 0;
}

#define RK3188_PULL_OFFSET		0x164
#define RK3188_PULL_BITS_PER_PIN	2
#define RK3188_PULL_PINS_PER_REG	8
#define RK3188_PULL_BANK_STRIDE		16
#define RK3188_PULL_PMU_OFFSET		0x64

static int rk3188_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	/* The first 12 pins of the first bank are located elsewhere */
	if (bank->bank_num == 0 && pin_num < 12) {
		*regmap = info->regmap_pmu ? info->regmap_pmu
					   : bank->regmap_pull;
		*reg = info->regmap_pmu ? RK3188_PULL_PMU_OFFSET : 0;
		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);
		*bit = pin_num % RK3188_PULL_PINS_PER_REG;
		*bit *= RK3188_PULL_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_pull ? info->regmap_pull
					    : info->regmap_base;
		*reg = info->regmap_pull ? 0 : RK3188_PULL_OFFSET;

		/* correct the offset, as it is the 2nd pull register */
		*reg -= 4;
		*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;
		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);

		/*
		 * The bits in these registers have an inverse ordering
		 * with the lowest pin being in bits 15:14 and the highest
		 * pin in bits 1:0
		 */
		*bit = 7 - (pin_num % RK3188_PULL_PINS_PER_REG);
		*bit *= RK3188_PULL_BITS_PER_PIN;
	}

	return 0;
}

#define RK3288_PULL_OFFSET		0x140
static int rk3288_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	/* The first 24 pins of the first bank are located in PMU */
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3188_PULL_PMU_OFFSET;

		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);
		*bit = pin_num % RK3188_PULL_PINS_PER_REG;
		*bit *= RK3188_PULL_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3288_PULL_OFFSET;

		/* correct the offset, as we're starting with the 2nd bank */
		*reg -= 0x10;
		*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;
		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3188_PULL_PINS_PER_REG);
		*bit *= RK3188_PULL_BITS_PER_PIN;
	}

	return 0;
}

#define RK3288_DRV_PMU_OFFSET		0x70
#define RK3288_DRV_GRF_OFFSET		0x1c0
#define RK3288_DRV_BITS_PER_PIN		2
#define RK3288_DRV_PINS_PER_REG		8
#define RK3288_DRV_BANK_STRIDE		16

static int rk3288_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	/* The first 24 pins of the first bank are located in PMU */
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3288_DRV_PMU_OFFSET;

		*reg += ((pin_num / RK3288_DRV_PINS_PER_REG) * 4);
		*bit = pin_num % RK3288_DRV_PINS_PER_REG;
		*bit *= RK3288_DRV_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3288_DRV_GRF_OFFSET;

		/* correct the offset, as we're starting with the 2nd bank */
		*reg -= 0x10;
		*reg += bank->bank_num * RK3288_DRV_BANK_STRIDE;
		*reg += ((pin_num / RK3288_DRV_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3288_DRV_PINS_PER_REG);
		*bit *= RK3288_DRV_BITS_PER_PIN;
	}

	return 0;
}

#define RK3228_PULL_OFFSET		0x100

static int rk3228_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3228_PULL_OFFSET;
	*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;
	*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);

	*bit = (pin_num % RK3188_PULL_PINS_PER_REG);
	*bit *= RK3188_PULL_BITS_PER_PIN;

	return 0;
}

#define RK3228_DRV_GRF_OFFSET		0x200

static int rk3228_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3228_DRV_GRF_OFFSET;
	*reg += bank->bank_num * RK3288_DRV_BANK_STRIDE;
	*reg += ((pin_num / RK3288_DRV_PINS_PER_REG) * 4);

	*bit = (pin_num % RK3288_DRV_PINS_PER_REG);
	*bit *= RK3288_DRV_BITS_PER_PIN;

	return 0;
}

#define RK3308_PULL_OFFSET		0xa0

static int rk3308_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3308_PULL_OFFSET;
	*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;
	*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);

	*bit = (pin_num % RK3188_PULL_PINS_PER_REG);
	*bit *= RK3188_PULL_BITS_PER_PIN;

	return 0;
}

#define RK3308_DRV_GRF_OFFSET		0x100

static int rk3308_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3308_DRV_GRF_OFFSET;
	*reg += bank->bank_num * RK3288_DRV_BANK_STRIDE;
	*reg += ((pin_num / RK3288_DRV_PINS_PER_REG) * 4);

	*bit = (pin_num % RK3288_DRV_PINS_PER_REG);
	*bit *= RK3288_DRV_BITS_PER_PIN;

	return 0;
}

#define RK3368_PULL_GRF_OFFSET		0x100
#define RK3368_PULL_PMU_OFFSET		0x10

static int rk3368_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	/* The first 32 pins of the first bank are located in PMU */
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3368_PULL_PMU_OFFSET;

		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);
		*bit = pin_num % RK3188_PULL_PINS_PER_REG;
		*bit *= RK3188_PULL_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3368_PULL_GRF_OFFSET;

		/* correct the offset, as we're starting with the 2nd bank */
		*reg -= 0x10;
		*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;
		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3188_PULL_PINS_PER_REG);
		*bit *= RK3188_PULL_BITS_PER_PIN;
	}

	return 0;
}

#define RK3368_DRV_PMU_OFFSET		0x20
#define RK3368_DRV_GRF_OFFSET		0x200

static int rk3368_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	/* The first 32 pins of the first bank are located in PMU */
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3368_DRV_PMU_OFFSET;

		*reg += ((pin_num / RK3288_DRV_PINS_PER_REG) * 4);
		*bit = pin_num % RK3288_DRV_PINS_PER_REG;
		*bit *= RK3288_DRV_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3368_DRV_GRF_OFFSET;

		/* correct the offset, as we're starting with the 2nd bank */
		*reg -= 0x10;
		*reg += bank->bank_num * RK3288_DRV_BANK_STRIDE;
		*reg += ((pin_num / RK3288_DRV_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3288_DRV_PINS_PER_REG);
		*bit *= RK3288_DRV_BITS_PER_PIN;
	}

	return 0;
}

#define RK3399_PULL_GRF_OFFSET		0xe040
#define RK3399_PULL_PMU_OFFSET		0x40
#define RK3399_DRV_3BITS_PER_PIN	3

static int rk3399_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	/* The bank0:16 and bank1:32 pins are located in PMU */
	if ((bank->bank_num == 0) || (bank->bank_num == 1)) {
		*regmap = info->regmap_pmu;
		*reg = RK3399_PULL_PMU_OFFSET;

		*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;

		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);
		*bit = pin_num % RK3188_PULL_PINS_PER_REG;
		*bit *= RK3188_PULL_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3399_PULL_GRF_OFFSET;

		/* correct the offset, as we're starting with the 3rd bank */
		*reg -= 0x20;
		*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;
		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3188_PULL_PINS_PER_REG);
		*bit *= RK3188_PULL_BITS_PER_PIN;
	}

	return 0;
}

static int rk3399_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	int drv_num = (pin_num / 8);

	/*  The bank0:16 and bank1:32 pins are located in PMU */
	if ((bank->bank_num == 0) || (bank->bank_num == 1))
		*regmap = info->regmap_pmu;
	else
		*regmap = info->regmap_base;

	*reg = bank->drv[drv_num].offset;
	if ((bank->drv[drv_num].drv_type == DRV_TYPE_IO_1V8_3V0_AUTO) ||
	    (bank->drv[drv_num].drv_type == DRV_TYPE_IO_3V3_ONLY))
		*bit = (pin_num % 8) * 3;
	else
		*bit = (pin_num % 8) * 2;

	return 0;
}

#define RK3568_PULL_PMU_OFFSET		0x20
#define RK3568_PULL_GRF_OFFSET		0x80
#define RK3568_PULL_BITS_PER_PIN	2
#define RK3568_PULL_PINS_PER_REG	8
#define RK3568_PULL_BANK_STRIDE		0x10

static int rk3568_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3568_PULL_PMU_OFFSET;
		*reg += bank->bank_num * RK3568_PULL_BANK_STRIDE;
		*reg += ((pin_num / RK3568_PULL_PINS_PER_REG) * 4);

		*bit = pin_num % RK3568_PULL_PINS_PER_REG;
		*bit *= RK3568_PULL_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3568_PULL_GRF_OFFSET;
		*reg += (bank->bank_num - 1) * RK3568_PULL_BANK_STRIDE;
		*reg += ((pin_num / RK3568_PULL_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3568_PULL_PINS_PER_REG);
		*bit *= RK3568_PULL_BITS_PER_PIN;
	}

	return 0;
}

#define RK3568_DRV_PMU_OFFSET		0x70
#define RK3568_DRV_GRF_OFFSET		0x200
#define RK3568_DRV_BITS_PER_PIN		8
#define RK3568_DRV_PINS_PER_REG		2
#define RK3568_DRV_BANK_STRIDE		0x40

static int rk3568_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	/* The first 32 pins of the first bank are located in PMU */
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3568_DRV_PMU_OFFSET;
		*reg += ((pin_num / RK3568_DRV_PINS_PER_REG) * 4);

		*bit = pin_num % RK3568_DRV_PINS_PER_REG;
		*bit *= RK3568_DRV_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3568_DRV_GRF_OFFSET;
		*reg += (bank->bank_num - 1) * RK3568_DRV_BANK_STRIDE;
		*reg += ((pin_num / RK3568_DRV_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3568_DRV_PINS_PER_REG);
		*bit *= RK3568_DRV_BITS_PER_PIN;
	}

	return 0;
}

#define RK3588_PMU1_IOC_REG		(0x0000)
#define RK3588_PMU2_IOC_REG		(0x4000)
#define RK3588_BUS_IOC_REG		(0x8000)
#define RK3588_VCCIO1_4_IOC_REG		(0x9000)
#define RK3588_VCCIO3_5_IOC_REG		(0xA000)
#define RK3588_VCCIO2_IOC_REG		(0xB000)
#define RK3588_VCCIO6_IOC_REG		(0xC000)
#define RK3588_EMMC_IOC_REG		(0xD000)

static const u32 rk3588_ds_regs[][2] = {
	{RK_GPIO0_A0, RK3588_PMU1_IOC_REG + 0x0010},
	{RK_GPIO0_A4, RK3588_PMU1_IOC_REG + 0x0014},
	{RK_GPIO0_B0, RK3588_PMU1_IOC_REG + 0x0018},
	{RK_GPIO0_B4, RK3588_PMU2_IOC_REG + 0x0014},
	{RK_GPIO0_C0, RK3588_PMU2_IOC_REG + 0x0018},
	{RK_GPIO0_C4, RK3588_PMU2_IOC_REG + 0x001C},
	{RK_GPIO0_D0, RK3588_PMU2_IOC_REG + 0x0020},
	{RK_GPIO0_D4, RK3588_PMU2_IOC_REG + 0x0024},
	{RK_GPIO1_A0, RK3588_VCCIO1_4_IOC_REG + 0x0020},
	{RK_GPIO1_A4, RK3588_VCCIO1_4_IOC_REG + 0x0024},
	{RK_GPIO1_B0, RK3588_VCCIO1_4_IOC_REG + 0x0028},
	{RK_GPIO1_B4, RK3588_VCCIO1_4_IOC_REG + 0x002C},
	{RK_GPIO1_C0, RK3588_VCCIO1_4_IOC_REG + 0x0030},
	{RK_GPIO1_C4, RK3588_VCCIO1_4_IOC_REG + 0x0034},
	{RK_GPIO1_D0, RK3588_VCCIO1_4_IOC_REG + 0x0038},
	{RK_GPIO1_D4, RK3588_VCCIO1_4_IOC_REG + 0x003C},
	{RK_GPIO2_A0, RK3588_EMMC_IOC_REG + 0x0040},
	{RK_GPIO2_A4, RK3588_VCCIO3_5_IOC_REG + 0x0044},
	{RK_GPIO2_B0, RK3588_VCCIO3_5_IOC_REG + 0x0048},
	{RK_GPIO2_B4, RK3588_VCCIO3_5_IOC_REG + 0x004C},
	{RK_GPIO2_C0, RK3588_VCCIO3_5_IOC_REG + 0x0050},
	{RK_GPIO2_C4, RK3588_VCCIO3_5_IOC_REG + 0x0054},
	{RK_GPIO2_D0, RK3588_EMMC_IOC_REG + 0x0058},
	{RK_GPIO2_D4, RK3588_EMMC_IOC_REG + 0x005C},
	{RK_GPIO3_A0, RK3588_VCCIO3_5_IOC_REG + 0x0060},
	{RK_GPIO3_A4, RK3588_VCCIO3_5_IOC_REG + 0x0064},
	{RK_GPIO3_B0, RK3588_VCCIO3_5_IOC_REG + 0x0068},
	{RK_GPIO3_B4, RK3588_VCCIO3_5_IOC_REG + 0x006C},
	{RK_GPIO3_C0, RK3588_VCCIO3_5_IOC_REG + 0x0070},
	{RK_GPIO3_C4, RK3588_VCCIO3_5_IOC_REG + 0x0074},
	{RK_GPIO3_D0, RK3588_VCCIO3_5_IOC_REG + 0x0078},
	{RK_GPIO3_D4, RK3588_VCCIO3_5_IOC_REG + 0x007C},
	{RK_GPIO4_A0, RK3588_VCCIO6_IOC_REG + 0x0080},
	{RK_GPIO4_A4, RK3588_VCCIO6_IOC_REG + 0x0084},
	{RK_GPIO4_B0, RK3588_VCCIO6_IOC_REG + 0x0088},
	{RK_GPIO4_B4, RK3588_VCCIO6_IOC_REG + 0x008C},
	{RK_GPIO4_C0, RK3588_VCCIO6_IOC_REG + 0x0090},
	{RK_GPIO4_C2, RK3588_VCCIO3_5_IOC_REG + 0x0090},
	{RK_GPIO4_C4, RK3588_VCCIO3_5_IOC_REG + 0x0094},
	{RK_GPIO4_D0, RK3588_VCCIO2_IOC_REG + 0x0098},
	{RK_GPIO4_D4, RK3588_VCCIO2_IOC_REG + 0x009C},
};

static const u32 rk3588_p_regs[][2] = {
	{RK_GPIO0_A0, RK3588_PMU1_IOC_REG + 0x0020},
	{RK_GPIO0_B0, RK3588_PMU1_IOC_REG + 0x0024},
	{RK_GPIO0_B5, RK3588_PMU2_IOC_REG + 0x0028},
	{RK_GPIO0_C0, RK3588_PMU2_IOC_REG + 0x002C},
	{RK_GPIO0_D0, RK3588_PMU2_IOC_REG + 0x0030},
	{RK_GPIO1_A0, RK3588_VCCIO1_4_IOC_REG + 0x0110},
	{RK_GPIO1_B0, RK3588_VCCIO1_4_IOC_REG + 0x0114},
	{RK_GPIO1_C0, RK3588_VCCIO1_4_IOC_REG + 0x0118},
	{RK_GPIO1_D0, RK3588_VCCIO1_4_IOC_REG + 0x011C},
	{RK_GPIO2_A0, RK3588_EMMC_IOC_REG + 0x0120},
	{RK_GPIO2_A6, RK3588_VCCIO3_5_IOC_REG + 0x0120},
	{RK_GPIO2_B0, RK3588_VCCIO3_5_IOC_REG + 0x0124},
	{RK_GPIO2_C0, RK3588_VCCIO3_5_IOC_REG + 0x0128},
	{RK_GPIO2_D0, RK3588_EMMC_IOC_REG + 0x012C},
	{RK_GPIO3_A0, RK3588_VCCIO3_5_IOC_REG + 0x0130},
	{RK_GPIO3_B0, RK3588_VCCIO3_5_IOC_REG + 0x0134},
	{RK_GPIO3_C0, RK3588_VCCIO3_5_IOC_REG + 0x0138},
	{RK_GPIO3_D0, RK3588_VCCIO3_5_IOC_REG + 0x013C},
	{RK_GPIO4_A0, RK3588_VCCIO6_IOC_REG + 0x0140},
	{RK_GPIO4_B0, RK3588_VCCIO6_IOC_REG + 0x0144},
	{RK_GPIO4_C0, RK3588_VCCIO6_IOC_REG + 0x0148},
	{RK_GPIO4_C2, RK3588_VCCIO3_5_IOC_REG + 0x0148},
	{RK_GPIO4_D0, RK3588_VCCIO2_IOC_REG + 0x014C},
};

static const u32 rk3588_smt_regs[][2] = {
	{RK_GPIO0_A0, RK3588_PMU1_IOC_REG + 0x0030},
	{RK_GPIO0_B0, RK3588_PMU1_IOC_REG + 0x0034},
	{RK_GPIO0_B5, RK3588_PMU2_IOC_REG + 0x0040},
	{RK_GPIO0_C0, RK3588_PMU2_IOC_REG + 0x0044},
	{RK_GPIO0_D0, RK3588_PMU2_IOC_REG + 0x0048},
	{RK_GPIO1_A0, RK3588_VCCIO1_4_IOC_REG + 0x0210},
	{RK_GPIO1_B0, RK3588_VCCIO1_4_IOC_REG + 0x0214},
	{RK_GPIO1_C0, RK3588_VCCIO1_4_IOC_REG + 0x0218},
	{RK_GPIO1_D0, RK3588_VCCIO1_4_IOC_REG + 0x021C},
	{RK_GPIO2_A0, RK3588_EMMC_IOC_REG + 0x0220},
	{RK_GPIO2_A6, RK3588_VCCIO3_5_IOC_REG + 0x0220},
	{RK_GPIO2_B0, RK3588_VCCIO3_5_IOC_REG + 0x0224},
	{RK_GPIO2_C0, RK3588_VCCIO3_5_IOC_REG + 0x0228},
	{RK_GPIO2_D0, RK3588_EMMC_IOC_REG + 0x022C},
	{RK_GPIO3_A0, RK3588_VCCIO3_5_IOC_REG + 0x0230},
	{RK_GPIO3_B0, RK3588_VCCIO3_5_IOC_REG + 0x0234},
	{RK_GPIO3_C0, RK3588_VCCIO3_5_IOC_REG + 0x0238},
	{RK_GPIO3_D0, RK3588_VCCIO3_5_IOC_REG + 0x023C},
	{RK_GPIO4_A0, RK3588_VCCIO6_IOC_REG + 0x0240},
	{RK_GPIO4_B0, RK3588_VCCIO6_IOC_REG + 0x0244},
	{RK_GPIO4_C0, RK3588_VCCIO6_IOC_REG + 0x0248},
	{RK_GPIO4_C2, RK3588_VCCIO3_5_IOC_REG + 0x0248},
	{RK_GPIO4_D0, RK3588_VCCIO2_IOC_REG + 0x024C},
};

#define RK3588_PULL_BITS_PER_PIN		2
#define RK3588_PULL_PINS_PER_REG		8

static int rk3588_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	u8 bank_num = bank->bank_num;
	u32 pin = bank_num * 32 + pin_num;
	int i;

	for (i = ARRAY_SIZE(rk3588_p_regs) - 1; i >= 0; i--) {
		if (pin >= rk3588_p_regs[i][0]) {
			*reg = rk3588_p_regs[i][1];
			*regmap = info->regmap_base;
			*bit = pin_num % RK3588_PULL_PINS_PER_REG;
			*bit *= RK3588_PULL_BITS_PER_PIN;
			return 0;
		}
	}

	return -EINVAL;
}

#define RK3588_DRV_BITS_PER_PIN		4
#define RK3588_DRV_PINS_PER_REG		4

static int rk3588_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	u8 bank_num = bank->bank_num;
	u32 pin = bank_num * 32 + pin_num;
	int i;

	for (i = ARRAY_SIZE(rk3588_ds_regs) - 1; i >= 0; i--) {
		if (pin >= rk3588_ds_regs[i][0]) {
			*reg = rk3588_ds_regs[i][1];
			*regmap = info->regmap_base;
			*bit = pin_num % RK3588_DRV_PINS_PER_REG;
			*bit *= RK3588_DRV_BITS_PER_PIN;
			return 0;
		}
	}

	return -EINVAL;
}

#define RK3588_SMT_BITS_PER_PIN		1
#define RK3588_SMT_PINS_PER_REG		8

static int rk3588_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
					   int pin_num,
					   struct regmap **regmap,
					   int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	u8 bank_num = bank->bank_num;
	u32 pin = bank_num * 32 + pin_num;
	int i;

	for (i = ARRAY_SIZE(rk3588_smt_regs) - 1; i >= 0; i--) {
		if (pin >= rk3588_smt_regs[i][0]) {
			*reg = rk3588_smt_regs[i][1];
			*regmap = info->regmap_base;
			*bit = pin_num % RK3588_SMT_PINS_PER_REG;
			*bit *= RK3588_SMT_BITS_PER_PIN;
			return 0;
		}
	}

	return -EINVAL;
}

static int rockchip_perpin_drv_list[DRV_TYPE_MAX][8] = {
	{ 2, 4, 8, 12, -1, -1, -1, -1 },
	{ 3, 6, 9, 12, -1, -1, -1, -1 },
	{ 5, 10, 15, 20, -1, -1, -1, -1 },
	{ 4, 6, 8, 10, 12, 14, 16, 18 },
	{ 4, 7, 10, 13, 16, 19, 22, 26 }
};

static int rockchip_set_drive_perpin(struct rockchip_pin_bank *bank,
				     int pin_num, int strength)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct device *dev = info->dev;
	struct regmap *regmap;
	int reg, ret, i;
	u32 data, rmask, rmask_bits, temp;
	u8 bit;
	int drv_type = bank->drv[pin_num / 8].drv_type;

	dev_dbg(dev, "setting drive of GPIO%d-%d to %d\n",
		bank->bank_num, pin_num, strength);

	ret = ctrl->drv_calc_reg(bank, pin_num, &regmap, &reg, &bit);
	if (ret)
		return ret;
	if (ctrl->type == RK3588) {
		rmask_bits = RK3588_DRV_BITS_PER_PIN;
		ret = strength;
		goto config;
	} else if (ctrl->type == RK3568) {
		rmask_bits = RK3568_DRV_BITS_PER_PIN;
		ret = (1 << (strength + 1)) - 1;
		goto config;
	}

	if (ctrl->type == RV1126) {
		rmask_bits = RV1126_DRV_BITS_PER_PIN;
		ret = strength;
		goto config;
	}

	ret = -EINVAL;
	for (i = 0; i < ARRAY_SIZE(rockchip_perpin_drv_list[drv_type]); i++) {
		if (rockchip_perpin_drv_list[drv_type][i] == strength) {
			ret = i;
			break;
		} else if (rockchip_perpin_drv_list[drv_type][i] < 0) {
			ret = rockchip_perpin_drv_list[drv_type][i];
			break;
		}
	}

	if (ret < 0) {
		dev_err(dev, "unsupported driver strength %d\n", strength);
		return ret;
	}

	switch (drv_type) {
	case DRV_TYPE_IO_1V8_3V0_AUTO:
	case DRV_TYPE_IO_3V3_ONLY:
		rmask_bits = RK3399_DRV_3BITS_PER_PIN;
		switch (bit) {
		case 0 ... 12:
			/* regular case, nothing to do */
			break;
		case 15:
			/*
			 * drive-strength offset is special, as it is spread
			 * over 2 registers, the bit data[15] contains bit 0
			 * of the value while temp[1:0] contains bits 2 and 1
			 */
			data = (ret & 0x1) << 15;
			temp = (ret >> 0x1) & 0x3;

			rmask = BIT(15) | BIT(31);
			data |= BIT(31);
			ret = regmap_update_bits(regmap, reg, rmask, data);
			if (ret)
				return ret;

			rmask = 0x3 | (0x3 << 16);
			temp |= (0x3 << 16);
			reg += 0x4;
			ret = regmap_update_bits(regmap, reg, rmask, temp);

			return ret;
		case 18 ... 21:
			/* setting fully enclosed in the second register */
			reg += 4;
			bit -= 16;
			break;
		default:
			dev_err(dev, "unsupported bit: %d for pinctrl drive type: %d\n",
				bit, drv_type);
			return -EINVAL;
		}
		break;
	case DRV_TYPE_IO_DEFAULT:
	case DRV_TYPE_IO_1V8_OR_3V0:
	case DRV_TYPE_IO_1V8_ONLY:
		rmask_bits = RK3288_DRV_BITS_PER_PIN;
		break;
	default:
		dev_err(dev, "unsupported pinctrl drive type: %d\n", drv_type);
		return -EINVAL;
	}

config:
	/* enable the write to the equivalent lower bits */
	data = ((1 << rmask_bits) - 1) << (bit + 16);
	rmask = data | (data >> 16);
	data |= (ret << bit);

	ret = regmap_update_bits(regmap, reg, rmask, data);

	return ret;
}

static int rockchip_pull_list[PULL_TYPE_MAX][4] = {
	{
		RK_BIAS_DISABLE,
		RK_BIAS_PULL_UP,
		RK_BIAS_PULL_DOWN,
		RK_BIAS_BUS_HOLD
	},
	{
		RK_BIAS_DISABLE,
		RK_BIAS_PULL_DOWN,
		RK_BIAS_DISABLE,
		RK_BIAS_PULL_UP
	},
};

static int rockchip_set_pull(struct rockchip_pin_bank *bank,
					int pin_num, int pull)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct device *dev = info->dev;
	struct regmap *regmap;
	int reg, ret, i, pull_type;
	u8 bit;
	u32 data, rmask;

	dev_dbg(dev, "setting pull of GPIO%d-%d to %d\n", bank->bank_num, pin_num, pull);

	/* rk3066b does support any pulls */
	if (ctrl->type == RK3066B)
		return pull ? -EINVAL : 0;

	ret = ctrl->pull_calc_reg(bank, pin_num, &regmap, &reg, &bit);
	if (ret)
		return ret;

	switch (ctrl->type) {
	case RK2928:
	case RK3128:
		data = BIT(bit + 16);
		if (pull == RK_BIAS_DISABLE)
			data |= BIT(bit);
		ret = regmap_write(regmap, reg, data);
		break;
	case PX30:
	case RV1108:
	case RV1126:
	case RK3188:
	case RK3288:
	case RK3308:
	case RK3368:
	case RK3399:
	case RK3568:
	case RK3588:
		pull_type = bank->pull_type[pin_num / 8];
		ret = -EINVAL;
		for (i = 0; i < ARRAY_SIZE(rockchip_pull_list[pull_type]); i++) {
			if (rockchip_pull_list[pull_type][i] == pull) {
				ret = i;
				break;
			}
		}
		/*
		 * In the TRM, pull-up being 1 for everything except the GPIO0_D3-D6,
		 * where that pull up value becomes 3.
		 */
		if (ctrl->type == RK3568 && bank->bank_num == 0 && pin_num >= 27 && pin_num <= 30) {
			if (ret == RK_BIAS_PULL_UP)
				ret = 3;
		}

		/* enable the write to the equivalent lower bits */
		data = ((1 << RK3188_PULL_BITS_PER_PIN) - 1) << (bit + 16);
		rmask = data | (data >> 16);
		data |= (ret << bit);

		ret = regmap_update_bits(regmap, reg, rmask, data);
		break;
	default:
		dev_err(dev, "unsupported pinctrl type\n");
		return -EINVAL;
	}

	return ret;
}

#define RK3328_SCHMITT_BITS_PER_PIN		1
#define RK3328_SCHMITT_PINS_PER_REG		16
#define RK3328_SCHMITT_BANK_STRIDE		8
#define RK3328_SCHMITT_GRF_OFFSET		0x380

static int rk3328_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
					   int pin_num,
					   struct regmap **regmap,
					   int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3328_SCHMITT_GRF_OFFSET;

	*reg += bank->bank_num * RK3328_SCHMITT_BANK_STRIDE;
	*reg += ((pin_num / RK3328_SCHMITT_PINS_PER_REG) * 4);
	*bit = pin_num % RK3328_SCHMITT_PINS_PER_REG;

	return 0;
}

#define RK3568_SCHMITT_BITS_PER_PIN		2
#define RK3568_SCHMITT_PINS_PER_REG		8
#define RK3568_SCHMITT_BANK_STRIDE		0x10
#define RK3568_SCHMITT_GRF_OFFSET		0xc0
#define RK3568_SCHMITT_PMUGRF_OFFSET		0x30

static int rk3568_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
					   int pin_num,
					   struct regmap **regmap,
					   int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3568_SCHMITT_PMUGRF_OFFSET;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3568_SCHMITT_GRF_OFFSET;
		*reg += (bank->bank_num - 1) * RK3568_SCHMITT_BANK_STRIDE;
	}

	*reg += ((pin_num / RK3568_SCHMITT_PINS_PER_REG) * 4);
	*bit = pin_num % RK3568_SCHMITT_PINS_PER_REG;
	*bit *= RK3568_SCHMITT_BITS_PER_PIN;

	return 0;
}

/*
 * Pinconf_ops handling
 */

static const struct of_device_id rockchip_pinctrl_dt_match[];

/* retrieve the soc specific data */
static struct rockchip_pin_ctrl *rockchip_pinctrl_get_soc_data(
						struct rockchip_pinctrl *d,
						struct device *dev)
{
	struct device_node *node = dev->of_node;
	const struct of_device_id *match;
	struct rockchip_pin_ctrl *ctrl;
	struct rockchip_pin_bank *bank;
	int grf_offs, pmu_offs, drv_grf_offs, drv_pmu_offs, i, j;

	match = of_match_node(rockchip_pinctrl_dt_match, node);
	ctrl = (struct rockchip_pin_ctrl *)match->data;

	grf_offs = ctrl->grf_mux_offset;
	pmu_offs = ctrl->pmu_mux_offset;
	drv_pmu_offs = ctrl->pmu_drv_offset;
	drv_grf_offs = ctrl->grf_drv_offset;
	bank = ctrl->pin_banks;
	for (i = 0; i < ctrl->nr_banks; ++i, ++bank) {
		int bank_pins = 0;

		bank->drvdata = d;
		bank->pin_base = ctrl->nr_pins;
		ctrl->nr_pins += bank->nr_pins;

		/* calculate iomux and drv offsets */
		for (j = 0; j < 4; j++) {
			struct rockchip_iomux *iom = &bank->iomux[j];
			struct rockchip_drv *drv = &bank->drv[j];
			int inc;

			if (bank_pins >= bank->nr_pins)
				break;

			/* preset iomux offset value, set new start value */
			if (iom->offset >= 0) {
				if ((iom->type & IOMUX_SOURCE_PMU) ||
				    (iom->type & IOMUX_L_SOURCE_PMU))
					pmu_offs = iom->offset;
				else
					grf_offs = iom->offset;
			} else { /* set current iomux offset */
				iom->offset = ((iom->type & IOMUX_SOURCE_PMU) ||
					       (iom->type & IOMUX_L_SOURCE_PMU)) ?
							pmu_offs : grf_offs;
			}

			/* preset drv offset value, set new start value */
			if (drv->offset >= 0) {
				if (iom->type & IOMUX_SOURCE_PMU)
					drv_pmu_offs = drv->offset;
				else
					drv_grf_offs = drv->offset;
			} else { /* set current drv offset */
				drv->offset = (iom->type & IOMUX_SOURCE_PMU) ?
						drv_pmu_offs : drv_grf_offs;
			}

			dev_dbg(dev, "bank %d, iomux %d has iom_offset 0x%x drv_offset 0x%x\n",
				i, j, iom->offset, drv->offset);

			/*
			 * Increase offset according to iomux width.
			 * 4bit iomux'es are spread over two registers.
			 */
			inc = (iom->type & (IOMUX_WIDTH_4BIT |
					    IOMUX_WIDTH_3BIT |
					    IOMUX_WIDTH_2BIT)) ? 8 : 4;
			if ((iom->type & IOMUX_SOURCE_PMU) || (iom->type & IOMUX_L_SOURCE_PMU))
				pmu_offs += inc;
			else
				grf_offs += inc;

			/*
			 * Increase offset according to drv width.
			 * 3bit drive-strenth'es are spread over two registers.
			 */
			if ((drv->drv_type == DRV_TYPE_IO_1V8_3V0_AUTO) ||
			    (drv->drv_type == DRV_TYPE_IO_3V3_ONLY))
				inc = 8;
			else
				inc = 4;

			if (iom->type & IOMUX_SOURCE_PMU)
				drv_pmu_offs += inc;
			else
				drv_grf_offs += inc;

			bank_pins += 8;
		}

		/* calculate the per-bank recalced_mask */
		for (j = 0; j < ctrl->niomux_recalced; j++) {
			int pin = 0;

			if (ctrl->iomux_recalced[j].num == bank->bank_num) {
				pin = ctrl->iomux_recalced[j].pin;
				bank->recalced_mask |= BIT(pin);
			}
		}

		/* calculate the per-bank route_mask */
		for (j = 0; j < ctrl->niomux_routes; j++) {
			int pin = 0;

			if (ctrl->iomux_routes[j].bank_num == bank->bank_num) {
				pin = ctrl->iomux_routes[j].pin;
				bank->route_mask |= BIT(pin);
			}
		}
	}

	return ctrl;
}

static int rockchip_pinctrl_set_state(struct pinctrl_device *pdev,
				      struct device_node *np)
{
	struct rockchip_pinctrl *info = to_rockchip_pinctrl(pdev);
	const __be32 *list;
	int i, size, ret;
	int bank_num, pin_num, func;

	/*
	 * the binding format is rockchip,pins = <bank pin mux CONFIG>,
	 * do sanity check and calculate pins number
	 */
	list = of_get_property(np, "rockchip,pins", &size);
	size /= sizeof(*list);

	if (!size || size % 4) {
		dev_err(pdev->dev, "wrong pins number or pins and configs should be by 4\n");
		return -EINVAL;
	}

	for (i = 0; i < size; i += 4) {
		const __be32 *phandle;
		struct device_node *np_config;
		struct rockchip_pin_bank *bank;
		u32 drive_strength;

		bank_num = be32_to_cpu(*list++);
		pin_num = be32_to_cpu(*list++);
		func = be32_to_cpu(*list++);
		phandle = list++;

		if (!phandle)
			return -EINVAL;

		np_config = of_find_node_by_phandle(be32_to_cpup(phandle));
		bank = bank_num_to_bank(info, bank_num);
		rockchip_set_mux(bank, pin_num, func);
		rockchip_set_pull(bank, pin_num, parse_bias_config(np_config));

		ret = of_property_read_u32(np_config, "drive-strength", &drive_strength);
		if (!ret)
			rockchip_set_drive_perpin(bank, pin_num, drive_strength);
	}

	return 0;
}

static struct pinctrl_ops rockchip_pinctrl_ops = {
	.set_state = rockchip_pinctrl_set_state,
};

static int rockchip_pinctrl_probe(struct device *dev)
{
	struct rockchip_pinctrl *info;
	struct device_node *np = dev->of_node, *node;
	struct rockchip_pin_ctrl *ctrl;
	int ret;

	if (!dev->of_node)
		return dev_err_probe(dev, -ENODEV, "device tree node not found\n");

	info = xzalloc(sizeof(*info));
	if (!info)
		return -ENOMEM;

	info->dev = dev;

	ctrl = rockchip_pinctrl_get_soc_data(info, dev);
	if (!ctrl)
		return dev_err_probe(dev, -EINVAL, "driver data not available\n");
	info->ctrl = ctrl;

	node = of_parse_phandle(np, "rockchip,grf", 0);
	if (!node)
		return -EINVAL;

	info->regmap_base = syscon_node_to_regmap(node);
	of_node_put(node);
	if (IS_ERR(info->regmap_base))
		return PTR_ERR(info->regmap_base);

	/* try to find the optional reference to the pmu syscon */
	node = of_parse_phandle(np, "rockchip,pmu", 0);
	if (node) {
		info->regmap_pmu = syscon_node_to_regmap(node);
		of_node_put(node);
		if (IS_ERR(info->regmap_pmu))
			return PTR_ERR(info->regmap_pmu);
	}

	dev->priv = info;

	ret = of_platform_populate(np, NULL, dev);
	if (ret)
		return dev_err_probe(dev, ret, "failed to register gpio device\n");

	info->pctl_dev.dev = dev;
	info->pctl_dev.ops = &rockchip_pinctrl_ops;

	ret = pinctrl_register(&info->pctl_dev);
	if (ret)
		return ret;

	return 0;
}

static __maybe_unused struct rockchip_pin_bank px30_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU
			    ),
	PIN_BANK_IOMUX_FLAGS(1, 32, "gpio1", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT
			    ),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT
			    ),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT
			    ),
};

static __maybe_unused struct rockchip_pin_ctrl px30_pin_ctrl = {
		.pin_banks		= px30_pin_banks,
		.nr_banks		= ARRAY_SIZE(px30_pin_banks),
		.label			= "PX30-GPIO",
		.type			= PX30,
		.grf_mux_offset		= 0x0,
		.pmu_mux_offset		= 0x0,
		.iomux_routes		= px30_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(px30_mux_route_data),
		.pull_calc_reg		= px30_calc_pull_reg_and_bit,
		.drv_calc_reg		= px30_calc_drv_reg_and_bit,
		.schmitt_calc_reg	= px30_calc_schmitt_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rv1108_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU),
	PIN_BANK_IOMUX_FLAGS(1, 32, "gpio1", 0, 0, 0, 0),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2", 0, 0, 0, 0),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3", 0, 0, 0, 0),
};

static __maybe_unused struct rockchip_pin_ctrl rv1108_pin_ctrl = {
	.pin_banks		= rv1108_pin_banks,
	.nr_banks		= ARRAY_SIZE(rv1108_pin_banks),
	.label			= "RV1108-GPIO",
	.type			= RV1108,
	.grf_mux_offset		= 0x10,
	.pmu_mux_offset		= 0x0,
	.iomux_recalced		= rv1108_mux_recalced_data,
	.niomux_recalced	= ARRAY_SIZE(rv1108_mux_recalced_data),
	.pull_calc_reg		= rv1108_calc_pull_reg_and_bit,
	.drv_calc_reg		= rv1108_calc_drv_reg_and_bit,
	.schmitt_calc_reg	= rv1108_calc_schmitt_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rv1126_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0",
			     IOMUX_WIDTH_4BIT | IOMUX_SOURCE_PMU,
			     IOMUX_WIDTH_4BIT | IOMUX_SOURCE_PMU,
			     IOMUX_WIDTH_4BIT | IOMUX_L_SOURCE_PMU,
			     IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS_OFFSET(1, 32, "gpio1",
				    IOMUX_WIDTH_4BIT,
				    IOMUX_WIDTH_4BIT,
				    IOMUX_WIDTH_4BIT,
				    IOMUX_WIDTH_4BIT,
				    0x10010, 0x10018, 0x10020, 0x10028),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2",
			     IOMUX_WIDTH_4BIT,
			     IOMUX_WIDTH_4BIT,
			     IOMUX_WIDTH_4BIT,
			     IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3",
			     IOMUX_WIDTH_4BIT,
			     IOMUX_WIDTH_4BIT,
			     IOMUX_WIDTH_4BIT,
			     IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(4, 2, "gpio4",
			     IOMUX_WIDTH_4BIT, 0, 0, 0),
};

static __maybe_unused struct rockchip_pin_ctrl rv1126_pin_ctrl = {
	.pin_banks		= rv1126_pin_banks,
	.nr_banks		= ARRAY_SIZE(rv1126_pin_banks),
	.label			= "RV1126-GPIO",
	.type			= RV1126,
	.grf_mux_offset		= 0x10004, /* mux offset from GPIO0_D0 */
	.pmu_mux_offset		= 0x0,
	.iomux_routes		= rv1126_mux_route_data,
	.niomux_routes		= ARRAY_SIZE(rv1126_mux_route_data),
	.iomux_recalced		= rv1126_mux_recalced_data,
	.niomux_recalced	= ARRAY_SIZE(rv1126_mux_recalced_data),
	.pull_calc_reg		= rv1126_calc_pull_reg_and_bit,
	.drv_calc_reg		= rv1126_calc_drv_reg_and_bit,
	.schmitt_calc_reg	= rv1126_calc_schmitt_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rk3036_pin_banks[] = {
	PIN_BANK(0, 32, "gpio0"),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
};

static __maybe_unused struct rockchip_pin_ctrl rk3036_pin_ctrl = {
		.pin_banks		= rk3036_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3036_pin_banks),
		.label			= "RK3036-GPIO",
		.type			= RK2928,
		.grf_mux_offset		= 0xa8,
		.pull_calc_reg		= rk2928_calc_pull_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rk3066a_pin_banks[] = {
	PIN_BANK(0, 32, "gpio0"),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
	PIN_BANK(3, 32, "gpio3"),
	PIN_BANK(4, 32, "gpio4"),
	PIN_BANK(6, 16, "gpio6"),
};

static __maybe_unused struct rockchip_pin_ctrl rk3066a_pin_ctrl = {
		.pin_banks		= rk3066a_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3066a_pin_banks),
		.label			= "RK3066a-GPIO",
		.type			= RK2928,
		.grf_mux_offset		= 0xa8,
		.pull_calc_reg		= rk2928_calc_pull_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rk3128_pin_banks[] = {
	PIN_BANK(0, 32, "gpio0"),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
	PIN_BANK(3, 32, "gpio3"),
};

static __maybe_unused struct rockchip_pin_ctrl rk3128_pin_ctrl = {
		.pin_banks		= rk3128_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3128_pin_banks),
		.label			= "RK3128-GPIO",
		.type			= RK3128,
		.grf_mux_offset		= 0xa8,
		.iomux_recalced		= rk3128_mux_recalced_data,
		.niomux_recalced	= ARRAY_SIZE(rk3128_mux_recalced_data),
		.iomux_routes		= rk3128_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3128_mux_route_data),
		.pull_calc_reg		= rk3128_calc_pull_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rk3188_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", IOMUX_GPIO_ONLY, 0, 0, 0),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
	PIN_BANK(3, 32, "gpio3"),
};

static __maybe_unused struct rockchip_pin_ctrl rk3188_pin_ctrl = {
		.pin_banks		= rk3188_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3188_pin_banks),
		.label			= "RK3188-GPIO",
		.type			= RK3188,
		.grf_mux_offset		= 0x60,
		.iomux_routes		= rk3188_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3188_mux_route_data),
		.pull_calc_reg		= rk3188_calc_pull_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rk3228_pin_banks[] = {
	PIN_BANK(0, 32, "gpio0"),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
	PIN_BANK(3, 32, "gpio3"),
};

static __maybe_unused struct rockchip_pin_ctrl rk3228_pin_ctrl = {
		.pin_banks		= rk3228_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3228_pin_banks),
		.label			= "RK3228-GPIO",
		.type			= RK3288,
		.grf_mux_offset		= 0x0,
		.iomux_routes		= rk3228_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3228_mux_route_data),
		.pull_calc_reg		= rk3228_calc_pull_reg_and_bit,
		.drv_calc_reg		= rk3228_calc_drv_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rk3288_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 24, "gpio0", IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_UNROUTED
			    ),
	PIN_BANK_IOMUX_FLAGS(1, 32, "gpio1", IOMUX_UNROUTED,
					     IOMUX_UNROUTED,
					     IOMUX_UNROUTED,
					     0
			    ),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2", 0, 0, 0, IOMUX_UNROUTED),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3", 0, 0, 0, IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(4, 32, "gpio4", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     0,
					     0
			    ),
	PIN_BANK_IOMUX_FLAGS(5, 32, "gpio5", IOMUX_UNROUTED,
					     0,
					     0,
					     IOMUX_UNROUTED
			    ),
	PIN_BANK_IOMUX_FLAGS(6, 32, "gpio6", 0, 0, 0, IOMUX_UNROUTED),
	PIN_BANK_IOMUX_FLAGS(7, 32, "gpio7", 0,
					     0,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_UNROUTED
			    ),
	PIN_BANK(8, 16, "gpio8"),
};

static __maybe_unused struct rockchip_pin_ctrl rk3288_pin_ctrl = {
		.pin_banks		= rk3288_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3288_pin_banks),
		.label			= "RK3288-GPIO",
		.type			= RK3288,
		.grf_mux_offset		= 0x0,
		.pmu_mux_offset		= 0x84,
		.iomux_routes		= rk3288_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3288_mux_route_data),
		.pull_calc_reg		= rk3288_calc_pull_reg_and_bit,
		.drv_calc_reg		= rk3288_calc_drv_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rk3308_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT),
	PIN_BANK_IOMUX_FLAGS(1, 32, "gpio1", IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2", IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3", IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT),
	PIN_BANK_IOMUX_FLAGS(4, 32, "gpio4", IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT),
};

static __maybe_unused struct rockchip_pin_ctrl rk3308_pin_ctrl = {
		.pin_banks		= rk3308_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3308_pin_banks),
		.label			= "RK3308-GPIO",
		.type			= RK3308,
		.grf_mux_offset		= 0x0,
		.iomux_recalced		= rk3308_mux_recalced_data,
		.niomux_recalced	= ARRAY_SIZE(rk3308_mux_recalced_data),
		.iomux_routes		= rk3308_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3308_mux_route_data),
		.pull_calc_reg		= rk3308_calc_pull_reg_and_bit,
		.drv_calc_reg		= rk3308_calc_drv_reg_and_bit,
		.schmitt_calc_reg	= rk3308_calc_schmitt_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rk3328_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", 0, 0, 0, 0),
	PIN_BANK_IOMUX_FLAGS(1, 32, "gpio1", 0, 0, 0, 0),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2", 0,
			     IOMUX_WIDTH_3BIT,
			     IOMUX_WIDTH_3BIT,
			     0),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3",
			     IOMUX_WIDTH_3BIT,
			     IOMUX_WIDTH_3BIT,
			     0,
			     0),
};

static __maybe_unused struct rockchip_pin_ctrl rk3328_pin_ctrl = {
		.pin_banks		= rk3328_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3328_pin_banks),
		.label			= "RK3328-GPIO",
		.type			= RK3288,
		.grf_mux_offset		= 0x0,
		.iomux_recalced		= rk3328_mux_recalced_data,
		.niomux_recalced	= ARRAY_SIZE(rk3328_mux_recalced_data),
		.iomux_routes		= rk3328_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3328_mux_route_data),
		.pull_calc_reg		= rk3228_calc_pull_reg_and_bit,
		.drv_calc_reg		= rk3228_calc_drv_reg_and_bit,
		.schmitt_calc_reg	= rk3328_calc_schmitt_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rk3368_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU
			    ),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
	PIN_BANK(3, 32, "gpio3"),
};

static __maybe_unused struct rockchip_pin_ctrl rk3368_pin_ctrl = {
		.pin_banks		= rk3368_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3368_pin_banks),
		.label			= "RK3368-GPIO",
		.type			= RK3368,
		.grf_mux_offset		= 0x0,
		.pmu_mux_offset		= 0x0,
		.pull_calc_reg		= rk3368_calc_pull_reg_and_bit,
		.drv_calc_reg		= rk3368_calc_drv_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rk3399_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS_DRV_FLAGS_OFFSET_PULL_FLAGS(0, 32, "gpio0",
							 IOMUX_SOURCE_PMU,
							 IOMUX_SOURCE_PMU,
							 IOMUX_SOURCE_PMU,
							 IOMUX_SOURCE_PMU,
							 DRV_TYPE_IO_1V8_ONLY,
							 DRV_TYPE_IO_1V8_ONLY,
							 DRV_TYPE_IO_DEFAULT,
							 DRV_TYPE_IO_DEFAULT,
							 0x80,
							 0x88,
							 -1,
							 -1,
							 PULL_TYPE_IO_1V8_ONLY,
							 PULL_TYPE_IO_1V8_ONLY,
							 PULL_TYPE_IO_DEFAULT,
							 PULL_TYPE_IO_DEFAULT
							),
	PIN_BANK_IOMUX_DRV_FLAGS_OFFSET(1, 32, "gpio1", IOMUX_SOURCE_PMU,
					IOMUX_SOURCE_PMU,
					IOMUX_SOURCE_PMU,
					IOMUX_SOURCE_PMU,
					DRV_TYPE_IO_1V8_OR_3V0,
					DRV_TYPE_IO_1V8_OR_3V0,
					DRV_TYPE_IO_1V8_OR_3V0,
					DRV_TYPE_IO_1V8_OR_3V0,
					0xa0,
					0xa8,
					0xb0,
					0xb8
					),
	PIN_BANK_DRV_FLAGS_PULL_FLAGS(2, 32, "gpio2", DRV_TYPE_IO_1V8_OR_3V0,
				      DRV_TYPE_IO_1V8_OR_3V0,
				      DRV_TYPE_IO_1V8_ONLY,
				      DRV_TYPE_IO_1V8_ONLY,
				      PULL_TYPE_IO_DEFAULT,
				      PULL_TYPE_IO_DEFAULT,
				      PULL_TYPE_IO_1V8_ONLY,
				      PULL_TYPE_IO_1V8_ONLY
				      ),
	PIN_BANK_DRV_FLAGS(3, 32, "gpio3", DRV_TYPE_IO_3V3_ONLY,
			   DRV_TYPE_IO_3V3_ONLY,
			   DRV_TYPE_IO_3V3_ONLY,
			   DRV_TYPE_IO_1V8_OR_3V0
			   ),
	PIN_BANK_DRV_FLAGS(4, 32, "gpio4", DRV_TYPE_IO_1V8_OR_3V0,
			   DRV_TYPE_IO_1V8_3V0_AUTO,
			   DRV_TYPE_IO_1V8_OR_3V0,
			   DRV_TYPE_IO_1V8_OR_3V0
			   ),
};

static __maybe_unused struct rockchip_pin_ctrl rk3399_pin_ctrl = {
		.pin_banks		= rk3399_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3399_pin_banks),
		.label			= "RK3399-GPIO",
		.type			= RK3399,
		.grf_mux_offset		= 0xe000,
		.pmu_mux_offset		= 0x0,
		.grf_drv_offset		= 0xe100,
		.pmu_drv_offset		= 0x80,
		.iomux_routes		= rk3399_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3399_mux_route_data),
		.pull_calc_reg		= rk3399_calc_pull_reg_and_bit,
		.drv_calc_reg		= rk3399_calc_drv_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rk3568_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", IOMUX_SOURCE_PMU | IOMUX_WIDTH_4BIT,
					     IOMUX_SOURCE_PMU | IOMUX_WIDTH_4BIT,
					     IOMUX_SOURCE_PMU | IOMUX_WIDTH_4BIT,
					     IOMUX_SOURCE_PMU | IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(1, 32, "gpio1", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(4, 32, "gpio4", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT),
};

static __maybe_unused struct rockchip_pin_ctrl rk3568_pin_ctrl = {
	.pin_banks		= rk3568_pin_banks,
	.nr_banks		= ARRAY_SIZE(rk3568_pin_banks),
	.label			= "RK3568-GPIO",
	.type			= RK3568,
	.grf_mux_offset		= 0x0,
	.pmu_mux_offset		= 0x0,
	.grf_drv_offset		= 0x0200,
	.pmu_drv_offset		= 0x0070,
	.iomux_routes		= rk3568_mux_route_data,
	.niomux_routes		= ARRAY_SIZE(rk3568_mux_route_data),
	.pull_calc_reg		= rk3568_calc_pull_reg_and_bit,
	.drv_calc_reg		= rk3568_calc_drv_reg_and_bit,
	.schmitt_calc_reg	= rk3568_calc_schmitt_reg_and_bit,
};

static __maybe_unused struct rockchip_pin_bank rk3588_pin_banks[] = {
	RK3588_PIN_BANK_FLAGS(0, 32, "gpio0",
			      IOMUX_WIDTH_4BIT, PULL_TYPE_IO_1V8_ONLY),
	RK3588_PIN_BANK_FLAGS(1, 32, "gpio1",
			      IOMUX_WIDTH_4BIT, PULL_TYPE_IO_1V8_ONLY),
	RK3588_PIN_BANK_FLAGS(2, 32, "gpio2",
			      IOMUX_WIDTH_4BIT, PULL_TYPE_IO_1V8_ONLY),
	RK3588_PIN_BANK_FLAGS(3, 32, "gpio3",
			      IOMUX_WIDTH_4BIT, PULL_TYPE_IO_1V8_ONLY),
	RK3588_PIN_BANK_FLAGS(4, 32, "gpio4",
			      IOMUX_WIDTH_4BIT, PULL_TYPE_IO_1V8_ONLY),
};

static __maybe_unused struct rockchip_pin_ctrl rk3588_pin_ctrl = {
	.pin_banks		= rk3588_pin_banks,
	.nr_banks		= ARRAY_SIZE(rk3588_pin_banks),
	.label			= "RK3588-GPIO",
	.type			= RK3588,
	.pull_calc_reg		= rk3588_calc_pull_reg_and_bit,
	.drv_calc_reg		= rk3588_calc_drv_reg_and_bit,
	.schmitt_calc_reg	= rk3588_calc_schmitt_reg_and_bit,
};

static const struct of_device_id rockchip_pinctrl_dt_match[] = {
#ifdef CONFIG_ARCH_PX30
	{ .compatible = "rockchip,px30-pinctrl",
		.data = &px30_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RV1108
	{ .compatible = "rockchip,rv1108-pinctrl",
		.data = &rv1108_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RV1126
	{ .compatible = "rockchip,rv1126-pinctrl",
		.data = &rv1126_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RK3036
	{ .compatible = "rockchip,rk3036-pinctrl",
		.data = &rk3036_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RK3066A
	{ .compatible = "rockchip,rk3066a-pinctrl",
		.data = &rk3066a_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RK3128
	{ .compatible = "rockchip,rk3128-pinctrl",
		.data = (void *)&rk3128_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RK3188
	{ .compatible = "rockchip,rk3188-pinctrl",
		.data = &rk3188_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RK3228
	{ .compatible = "rockchip,rk3228-pinctrl",
		.data = &rk3228_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RK3288
	{ .compatible = "rockchip,rk3288-pinctrl",
		.data = &rk3288_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RK3308
	{ .compatible = "rockchip,rk3308-pinctrl",
		.data = &rk3308_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RK3328
	{ .compatible = "rockchip,rk3328-pinctrl",
		.data = &rk3328_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RK3368
	{ .compatible = "rockchip,rk3368-pinctrl",
		.data = &rk3368_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RK3399
	{ .compatible = "rockchip,rk3399-pinctrl",
		.data = &rk3399_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RK3568
	{ .compatible = "rockchip,rk3568-pinctrl",
		.data = &rk3568_pin_ctrl },
#endif
#ifdef CONFIG_ARCH_RK3588
	{ .compatible = "rockchip,rk3588-pinctrl",
		.data = &rk3588_pin_ctrl },
#endif
	{},
};
MODULE_DEVICE_TABLE(of, rockchip_pinctrl_dt_match);

static struct driver rockchip_pinctrl_driver = {
	.name = "rockchip-pinctrl",
	.probe = rockchip_pinctrl_probe,
	.of_compatible = DRV_OF_COMPAT(rockchip_pinctrl_dt_match),
};

core_platform_driver(rockchip_pinctrl_driver);
