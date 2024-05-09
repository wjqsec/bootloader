/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MACH_STM32MP_BBU_H_
#define MACH_STM32MP_BBU_H_

#include <bbu.h>

static inline int stm32mp_bbu_mmc_register_handler(const char *name,
						   const char *devicefile,
						   unsigned long flags)
{
	return bbu_register_std_file_update(name, flags, devicefile,
					    filetype_stm32_image_ssbl_v1);
}

#ifdef CONFIG_BAREBOX_UPDATE

int stm32mp_bbu_mmc_fip_register(const char *name, const char *devicefile,
				 unsigned long flags);

#else

static inline int stm32mp_bbu_mmc_fip_register(const char *name,
					       const char *devicefile,
					       unsigned long flags)
{
	return -ENOSYS;
}

#endif

#endif /* MACH_STM32MP_BBU_H_ */
