// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2018, STMicroelectronics - All Rights Reserved
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#define pr_fmt(fmt) "stm32mp-init: " fmt

#include <common.h>
#include <init.h>
#include <mach/stm32mp/stm32.h>
#include <mach/stm32mp/bsec.h>
#include <mach/stm32mp/revision.h>
#include <mach/stm32mp/bootsource.h>
#include <bootsource.h>
#include <dt-bindings/pinctrl/stm32-pinfunc.h>

/* Package = bit 27:29 of OTP16
 * - 100: LBGA448  (FFI) => AA = LFBGA 18x18mm 448 balls p. 0.8mm
 * - 011: LBGA354  (LCI) => AB = LFBGA 16x16mm 359 balls p. 0.8mm
 * - 010: TFBGA361 (FFC) => AC = TFBGA 12x12mm 361 balls p. 0.5mm
 * - 001: TFBGA257 (LCC) => AD = TFBGA 10x10mm 257 balls p. 0.5mm
 * - others: Reserved
 */
#define PKG_SHIFT	27
#define PKG_MASK	GENMASK(2, 0)

#define PKG_AA_LBGA448	4
#define PKG_AB_LBGA354	3
#define PKG_AC_TFBGA361	2
#define PKG_AD_TFBGA257	1

/*
 * enumerated for boot interface from Bootrom, used in TAMP_BOOT_CONTEXT
 * - boot device = bit 8:4
 * - boot instance = bit 3:0
 */
#define BOOT_TYPE_MASK		0xF0
#define BOOT_TYPE_SHIFT		4
#define BOOT_INSTANCE_MASK	0x0F
#define BOOT_INSTANCE_SHIFT	0

/* TAMP registers */
#define TAMP_BACKUP_REGISTER(x)         (STM32_TAMP_BASE + 0x100 + 4 * x)
/* non secure access */
#define STM32MP13_TAMP_BOOT_CONTEXT     TAMP_BACKUP_REGISTER(30)
#define STM32MP15_TAMP_BOOT_CONTEXT     TAMP_BACKUP_REGISTER(20)

#define TAMP_BOOT_MODE_MASK             GENMASK(15, 8)
#define TAMP_BOOT_MODE_SHIFT            8
#define TAMP_BOOT_DEVICE_MASK           GENMASK(7, 4)
#define TAMP_BOOT_INSTANCE_MASK         GENMASK(3, 0)
#define TAMP_BOOT_FORCED_MASK           GENMASK(7, 0)
#define TAMP_BOOT_DEBUG_ON              BIT(16)

#define FIXUP_CPU_MASK(num, mhz) (((num) << 16) | (mhz))
#define FIXUP_CPU_NUM(mask) ((mask) >> 16)
#define FIXUP_CPU_HZ(mask) (((mask) & GENMASK(15, 0)) * 1000UL * 1000UL)

static void setup_boot_mode(u32 boot_ctx)
{
	u32 boot_mode =
		(boot_ctx & TAMP_BOOT_MODE_MASK) >> TAMP_BOOT_MODE_SHIFT;
	int instance = (boot_mode & TAMP_BOOT_INSTANCE_MASK) - 1;
	enum bootsource src = BOOTSOURCE_UNKNOWN;

	switch (boot_mode & TAMP_BOOT_DEVICE_MASK) {
	case STM32MP_BOOT_SERIAL_UART:
		src = BOOTSOURCE_SERIAL;
		break;
	case STM32MP_BOOT_SERIAL_USB:
		src = BOOTSOURCE_USB;
		break;
	case STM32MP_BOOT_FLASH_SD:
	case STM32MP_BOOT_FLASH_EMMC:
		src = BOOTSOURCE_MMC;
		break;
	case STM32MP_BOOT_FLASH_NAND:
		src = BOOTSOURCE_NAND;
		break;
	case STM32MP_BOOT_FLASH_NOR:
		instance = 0;
		src = BOOTSOURCE_NOR;
		break;
	case STM32MP_BOOT_FLASH_NOR_QSPI:
		instance--;
		src = BOOTSOURCE_SPI_NOR;
		break;
	default:
		pr_debug("unexpected boot mode\n");
		break;
	}

	pr_debug("[boot_ctx=0x%x] => mode=0x%x, instance=%d\n",
		 boot_ctx, boot_mode, instance);

	bootsource_set_raw(src, instance);
}

static int get_cpu_package(u32 *pkg)
{
	int ret = bsec_read_field(BSEC_OTP_PKG, pkg);
	if (ret)
		return ret;

	*pkg = (*pkg >> PKG_SHIFT) & PKG_MASK;
	return 0;
}

static int stm32mp15_fixup_cpus(struct device_node *root, void *_ctx)
{
	unsigned long ctx = (unsigned long)_ctx;
	struct device_node *cpus_node, *np, *tmp;

	cpus_node = of_find_node_by_name_address(root, "cpus");
	if (!cpus_node)
		return 0;

	for_each_child_of_node_safe(cpus_node, tmp, np) {
		u32 cpu_index;

		if (of_property_read_u32(np, "reg", &cpu_index))
			continue;

		if (cpu_index >= FIXUP_CPU_NUM(ctx)) {
			of_delete_node(np);
			continue;
		}

		of_property_write_u32(np, "clock-frequency", FIXUP_CPU_HZ(ctx));
	}

	return 0;
}

static int fixup_pinctrl(struct device_node *root, const char *compat, u32 pkg)
{
	struct device_node *np = of_find_compatible_node(root, NULL, compat);
	if (!np)
		return -ENODEV;

	return of_property_write_u32(np, "st,package", pkg);
}

static int stm32mp15_fixup_pkg(struct device_node *root, void *_pkg)
{
	unsigned long pkg = (unsigned long)_pkg;
	int ret;

	ret = fixup_pinctrl(root, "st,stm32mp157-pinctrl", pkg);
	if (ret)
		return ret;

	return fixup_pinctrl(root, "st,stm32mp157-z-pinctrl", pkg);
}

static int stm32mp15_setup_cpu_type(void)
{
	unsigned long cpufixupctx = 0, pkgfixupctx = 0;
	int cputype, package;

	__stm32mp15_get_cpu_type(&cputype);
	switch (cputype) {
	case CPU_STM32MP157Fxx:
		cpufixupctx = FIXUP_CPU_MASK(2, 800);
		break;
	case CPU_STM32MP157Dxx:
		cpufixupctx = FIXUP_CPU_MASK(2, 800);
		break;
	case CPU_STM32MP157Cxx:
		cpufixupctx = FIXUP_CPU_MASK(2, 650);
		break;
	case CPU_STM32MP157Axx:
		cpufixupctx = FIXUP_CPU_MASK(2, 650);
		break;
	case CPU_STM32MP153Fxx:
		cpufixupctx = FIXUP_CPU_MASK(2, 800);
		break;
	case CPU_STM32MP153Dxx:
		cpufixupctx = FIXUP_CPU_MASK(2, 800);
		break;
	case CPU_STM32MP153Cxx:
		cpufixupctx = FIXUP_CPU_MASK(2, 650);
		break;
	case CPU_STM32MP153Axx:
		cpufixupctx = FIXUP_CPU_MASK(2, 650);
		break;
	case CPU_STM32MP151Cxx:
		cpufixupctx = FIXUP_CPU_MASK(1, 650);
		break;
	case CPU_STM32MP151Axx:
		cpufixupctx = FIXUP_CPU_MASK(1, 650);
		break;
	case CPU_STM32MP151Fxx:
		cpufixupctx = FIXUP_CPU_MASK(1, 800);
		break;
	case CPU_STM32MP151Dxx:
		cpufixupctx = FIXUP_CPU_MASK(1, 800);
		break;
	default:
		break;
	}

	get_cpu_package(&package);
	switch (package) {
	case PKG_AA_LBGA448:
		pkgfixupctx = STM32MP_PKG_AA;
		break;
	case PKG_AB_LBGA354:
		pkgfixupctx = STM32MP_PKG_AB;
		break;
	case PKG_AC_TFBGA361:
		pkgfixupctx = STM32MP_PKG_AC;
		break;
	case PKG_AD_TFBGA257:
		pkgfixupctx = STM32MP_PKG_AD;
		break;
	default:
		break;
	}

	pr_debug("cputype = 0x%x, package = 0x%x\n", cputype, package);

	if (cpufixupctx)
		of_register_fixup(stm32mp15_fixup_cpus, (void*)cpufixupctx);
	if (pkgfixupctx)
		of_register_fixup(stm32mp15_fixup_pkg, (void*)pkgfixupctx);

	return 0;
}

static int __st32mp_soc;

int stm32mp_soc(void)
{
	return __st32mp_soc;
}

static int stm32mp_init(void)
{
	u32 boot_ctx;

	if (of_machine_is_compatible("st,stm32mp135"))
		__st32mp_soc = 32135;
	else if (of_machine_is_compatible("st,stm32mp151"))
		__st32mp_soc = 32151;
	else if (of_machine_is_compatible("st,stm32mp153"))
		__st32mp_soc = 32153;
	else if (of_machine_is_compatible("st,stm32mp157"))
		__st32mp_soc = 32157;
	else
		return 0;

	if (__st32mp_soc == 32135) {
		boot_ctx = readl(STM32MP13_TAMP_BOOT_CONTEXT);
	} else {
		stm32mp15_setup_cpu_type();
		boot_ctx = readl(STM32MP15_TAMP_BOOT_CONTEXT);
	}

	setup_boot_mode(boot_ctx);

	return 0;
}
postcore_initcall(stm32mp_init);
