// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2014 Lothar Waßmann <LW@KARO-electronics.de>

#include <common.h>
#include <i2c/i2c.h>
#include "pmic.h"

#define LTC3676_BUCK1		0x01
#define LTC3676_BUCK2		0x02
#define LTC3676_BUCK3		0x03
#define LTC3676_BUCK4		0x04
#define LTC3676_DVB1A		0x0A
#define LTC3676_DVB1B		0x0B
#define LTC3676_DVB2A		0x0C
#define LTC3676_DVB2B		0x0D
#define LTC3676_DVB3A		0x0E
#define LTC3676_DVB3B		0x0F
#define LTC3676_DVB4A		0x10
#define LTC3676_DVB4B		0x11
#define LTC3676_MSKPG		0x13
#define LTC3676_CLIRQ		0x1f

#define LTC3676_BUCK_DVDT_FAST	(1 << 0)
#define LTC3676_BUCK_KEEP_ALIVE	(1 << 1)
#define LTC3676_BUCK_CLK_RATE_LOW (1 << 2)
#define LTC3676_BUCK_PHASE_SEL	(1 << 3)
#define LTC3676_BUCK_ENABLE_300	(1 << 4)
#define LTC3676_BUCK_PULSE_SKIP	(0 << 5)
#define LTC3676_BUCK_BURST_MODE	(1 << 5)
#define LTC3676_BUCK_CONTINUOUS	(2 << 5)
#define LTC3676_BUCK_ENABLE	(1 << 7)

#define LTC3676_PGOOD_MASK	(1 << 5)

#define LTC3676_MSKPG_BUCK1	(1 << 0)
#define LTC3676_MSKPG_BUCK2	(1 << 1)
#define LTC3676_MSKPG_BUCK3	(1 << 2)
#define LTC3676_MSKPG_BUCK4	(1 << 3)
#define LTC3676_MSKPG_LDO2	(1 << 5)
#define LTC3676_MSKPG_LDO3	(1 << 6)
#define LTC3676_MSKPG_LDO4	(1 << 7)

#define VDD_IO_VAL		mV_to_regval(vout_to_vref(3300 * 10, 5))
#define VDD_IO_VAL_LP		mV_to_regval(vout_to_vref(3100 * 10, 5))
#define VDD_IO_VAL_2		mV_to_regval(vout_to_vref(3300 * 10, 5_2))
#define VDD_IO_VAL_2_LP		mV_to_regval(vout_to_vref(3100 * 10, 5_2))
#define VDD_SOC_VAL		mV_to_regval(vout_to_vref(1425 * 10, 6))
#define VDD_SOC_VAL_LP		mV_to_regval(vout_to_vref(900 * 10, 6))
#define VDD_DDR_VAL		mV_to_regval(vout_to_vref(1500 * 10, 7))
#define VDD_DDR_VAL_LP		mV_to_regval(vout_to_vref(1500 * 10, 7))
#define VDD_CORE_VAL		mV_to_regval(vout_to_vref(1425 * 10, 8))
#define VDD_CORE_VAL_LP		mV_to_regval(vout_to_vref(900 * 10, 8))

/* LDO1 */
#define R1_1			470
#define R2_1			150
/* LDO4 */
#define R1_4			470
#define R2_4			150
/* Buck1 */
#define R1_5			390
#define R2_5			110
#define R1_5_2			470
#define R2_5_2			150
/* Buck2 (SOC) */
#define R1_6			150
#define R2_6			180
/* Buck3 (DDR) */
#define R1_7			150
#define R2_7			140
/* Buck4 (CORE) */
#define R1_8			150
#define R2_8			180

/* calculate voltages in 10mV */
#define R1(idx)			R1_##idx
#define R2(idx)			R2_##idx

#define vout_to_vref(vout, idx)	((vout) * R2(idx) / (R1(idx) + R2(idx)))
#define vref_to_vout(vref, idx)	DIV_ROUND_UP((vref) * (R1(idx) + R2(idx)), R2(idx))

#define mV_to_regval(mV)	DIV_ROUND(((((mV) < 4125) ? 4125 : (mV)) - 4125), 125)
#define regval_to_mV(v)		(((v) * 125 + 4125))

static struct ltc3673_regs {
	u8 addr;
	u8 val;
	u8 mask;
} ltc3676_regs[] = {
	{ LTC3676_MSKPG, ~LTC3676_MSKPG_BUCK1, },
	{ LTC3676_DVB2B, VDD_SOC_VAL_LP | LTC3676_PGOOD_MASK, ~0x3f, },
	{ LTC3676_DVB3B, VDD_DDR_VAL_LP, ~0x3f, },
	{ LTC3676_DVB4B, VDD_CORE_VAL_LP | LTC3676_PGOOD_MASK, ~0x3f, },
	{ LTC3676_DVB2A, VDD_SOC_VAL, ~0x3f, },
	{ LTC3676_DVB3A, VDD_DDR_VAL, ~0x3f, },
	{ LTC3676_DVB4A, VDD_CORE_VAL, ~0x3f, },
	{ LTC3676_BUCK1, LTC3676_BUCK_BURST_MODE | LTC3676_BUCK_CLK_RATE_LOW, },
	{ LTC3676_BUCK2, LTC3676_BUCK_BURST_MODE, },
	{ LTC3676_BUCK3, LTC3676_BUCK_BURST_MODE, },
	{ LTC3676_BUCK4, LTC3676_BUCK_BURST_MODE, },
	{ LTC3676_CLIRQ, 0, }, /* clear interrupt status */
};

static struct ltc3673_regs ltc3676_regs_2[] = {
	{ LTC3676_DVB1B, VDD_IO_VAL_2_LP | LTC3676_PGOOD_MASK, ~0x3f, },
	{ LTC3676_DVB1A, VDD_IO_VAL_2, ~0x3f, },
};


int ltc3676_pmic_setup(struct i2c_client *client)
{
	int i;
	struct ltc3673_regs *r;

	r = ltc3676_regs;

	for (i = 0; i < ARRAY_SIZE(ltc3676_regs); i++, r++) {
		if (i2c_write_reg(client, r->addr, &r->val, 1) != 1) {
			pr_err("i2c write error\n");
			return -EIO;
		}
	}

	r = ltc3676_regs_2;

	for (i = 0; i < ARRAY_SIZE(ltc3676_regs_2); i++, r++) {
		if (i2c_write_reg(client, r->addr, &r->val, 1) != 1) {
			pr_err("i2c write error\n");
			return -EIO;
		}
	}

	return 0;
}

