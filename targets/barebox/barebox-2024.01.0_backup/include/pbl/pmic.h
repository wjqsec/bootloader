/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __PBL_PMIC_H_
#define __PBL_PMIC_H_

#include <pbl/i2c.h>

struct pmic_config {
	u8 reg;
	u8 val;
};

static void pmic_reg_write(struct pbl_i2c *i2c, int addr, u8 reg, u8 val)
{
	int ret;
	u8 buf[32];
	struct i2c_msg msgs[] = {
		{
			.addr = addr,
			.buf = buf,
		},
	};

	buf[0] = reg;
	buf[1] = val;

	msgs[0].len = 2;

	ret = pbl_i2c_xfer(i2c, msgs, ARRAY_SIZE(msgs));
	if (ret != 1)
		pr_err("Failed to write to pmic@%x: %d\n", addr, ret);
}

static inline void pmic_configure(struct pbl_i2c *i2c, u8 addr,
				  const struct pmic_config *config,
				  size_t config_len)
{
	for (; config_len--; config++)
		pmic_reg_write(i2c, addr, config->reg, config->val);
}

#endif
