// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2013 Thomas Petazzoni <thomas.petazzoni@free-electrons.com>
// SPDX-FileCopyrightText: 2013 Sebastian Hesselbarth <sebastian.hesselbarth@gmail.com>

#include <common.h>
#include <init.h>
#include <io.h>
#include <of.h>
#include <of_address.h>
#include <linux/clk.h>
#include <mach/mvebu/common.h>
#include <mach/mvebu/socid.h>
#include <asm/barebox-arm.h>
#include <asm/memory.h>
#include <mach/mvebu/lowlevel.h>

/*
 * The different SoC headers containing register definitions (mach/dove-regs.h,
 * mach/kirkwood-regs.h and mach/armada-370-xp-regs.h) are pairwise
 * incompatible. So some defines are reproduced here instead of just #included.
 */

#define DOVE_SDRAM_BASE			IOMEM(0xf1800000)
#define DOVE_SDRAM_MAPn(n)		(0x100 + ((n) * 0x10))
#define DOVE_SDRAM_MAP_VALID		BIT(0)
#define DOVE_SDRAM_LENGTH_SHIFT		16
#define DOVE_SDRAM_LENGTH_MASK		(0x00f << DOVE_SDRAM_LENGTH_SHIFT)
#define DOVE_SDRAM_REGS_BASE_DECODE	0x10

#define DOVE_CPU_CTRL			(MVEBU_REMAP_INT_REG_BASE + 0xd025c)
#define DOVE_AXI_CTRL			(MVEBU_REMAP_INT_REG_BASE + 0xd0224)

#define KIRKWOOD_SDRAM_BASE		(IOMEM(MVEBU_REMAP_INT_REG_BASE) + 0x00000)
#define KIRKWOOD_DDR_BASE_CSn(n)	(0x1500 + ((n) * 0x8))
#define KIRKWOOD_DDR_SIZE_CSn(n)	(0x1504 + ((n) * 0x8))
#define KIRKWOOD_DDR_SIZE_ENABLED	BIT(0)
#define KIRKWOOD_DDR_SIZE_MASK		0xff000000

#define ARMADA_370_XP_SDRAM_BASE	(IOMEM(MVEBU_REMAP_INT_REG_BASE) + 0x20000)
#define ARMADA_370_XP_DDR_SIZE_CSn(n)	(0x184 + ((n) * 0x8))
#define ARMADA_370_XP_DDR_SIZE_ENABLED	BIT(0)
#define ARMADA_370_XP_DDR_SIZE_MASK	0xffff0000

/*
 * Marvell MVEBU SoC id and revision can be read from any PCIe
 * controller port.
 */
u16 soc_devid;
EXPORT_SYMBOL(soc_devid);
u16 soc_revid;
EXPORT_SYMBOL(soc_revid);

static const struct of_device_id mvebu_pcie_of_ids[] = {
	{ .compatible = "marvell,armada-xp-pcie", },
	{ .compatible = "marvell,armada-370-pcie", },
	{ .compatible = "marvell,dove-pcie" },
	{ .compatible = "marvell,kirkwood-pcie" },
	{ },
};
MODULE_DEVICE_TABLE(of, mvebu_pcie_of_ids);

static int mvebu_soc_id_init(void)
{
	struct device_node *np, *cnp;
	struct clk *clk;
	void __iomem *base;

	np = of_find_matching_node(NULL, mvebu_pcie_of_ids);
	if (!np)
		return -ENODEV;

	for_each_child_of_node(np, cnp) {
		base = of_iomap(cnp, 0);
		if (!base)
			continue;

		clk = of_clk_get(cnp, 0);
		if (IS_ERR(clk))
			continue;

		clk_enable(clk);
		soc_devid = readl(base + PCIE_VEN_DEV_ID) >> 16;
		soc_revid = readl(base + PCIE_REV_ID) & REV_ID_MASK;
		clk_disable(clk);
		break;
	}

	if (!soc_devid) {
		pr_err("Unable to read SoC id from PCIe ports\n");
		return -EINVAL;
	}

	pr_info("SoC: Marvell %04x rev %d\n", soc_devid, soc_revid);

	return 0;
}
postcore_initcall(mvebu_soc_id_init);

static unsigned long dove_memory_find(void)
{
	int n;
	unsigned long mem_size = 0;

	for (n = 0; n < 2; n++) {
		uint32_t map = readl(DOVE_SDRAM_BASE + DOVE_SDRAM_MAPn(n));
		uint32_t size;

		/* skip disabled areas */
		if ((map & DOVE_SDRAM_MAP_VALID) != DOVE_SDRAM_MAP_VALID)
			continue;

		/* real size is encoded as ld(2^(16+length)) */
		size = (map & DOVE_SDRAM_LENGTH_MASK) >> DOVE_SDRAM_LENGTH_SHIFT;
		mem_size += 1 << (16 + size);
	}

	return mem_size;
}

static unsigned long kirkwood_memory_find(void)
{
	int cs;
	unsigned long mem_size = 0;

	for (cs = 0; cs < 4; cs++) {
		u32 ctrl = readl(KIRKWOOD_SDRAM_BASE +
				 KIRKWOOD_DDR_SIZE_CSn(cs));

		/* Skip non-enabled CS */
		if ((ctrl & KIRKWOOD_DDR_SIZE_ENABLED) !=
		    KIRKWOOD_DDR_SIZE_ENABLED)
			continue;

		mem_size += (ctrl | ~KIRKWOOD_DDR_SIZE_MASK) + 1;
	}

	return mem_size;
}

static unsigned long armada_370_xp_memory_find(void)
{
	int cs;
	unsigned long mem_size = 0;

	for (cs = 0; cs < 4; cs++) {
		u32 ctrl = readl(ARMADA_370_XP_SDRAM_BASE + ARMADA_370_XP_DDR_SIZE_CSn(cs));

		/* Skip non-enabled CS */
		if ((ctrl & ARMADA_370_XP_DDR_SIZE_ENABLED) != ARMADA_370_XP_DDR_SIZE_ENABLED)
			continue;

		mem_size += (ctrl | ~ARMADA_370_XP_DDR_SIZE_MASK) + 1;
	}

	return mem_size;
}

static int mvebu_meminit(void)
{
	if (of_machine_is_compatible("marvell,armada-370-xp"))
		arm_add_mem_device("ram0", 0, armada_370_xp_memory_find());

	return 0;
}
mem_initcall(mvebu_meminit);

/*
 * All MVEBU SoCs start with internal registers at 0xd0000000.
 * To get more contiguous address space and as Linux expects them
 * there, we remap them early to 0xf1000000.
 *
 * There no way to determine internal registers base address
 * safely later on, as the remap register itself is within the
 * internal registers.
 */
#define MVEBU_BRIDGE_REG_BASE		0x20000
#define DEVICE_INTERNAL_BASE_ADDR	(MVEBU_BRIDGE_REG_BASE + 0x80)

static __always_inline void mvebu_remap_registers(void)
{
	void __iomem *base = mvebu_get_initial_int_reg_base();

	writel(MVEBU_REMAP_INT_REG_BASE, base + DEVICE_INTERNAL_BASE_ADDR);
}

void __naked __noreturn dove_barebox_entry(void *boarddata)
{
	uint32_t val;
	void __iomem *mcbase = mvebu_get_initial_int_reg_base() + 0x800000;

	mvebu_remap_registers();

	/*
	 * On dove there is an additional register window that is expected to be
	 * located 0x800000 after the main register window. This contains the
	 * DDR registers.
	 */
	val = readl(mcbase + DOVE_SDRAM_REGS_BASE_DECODE) & 0x0000ffff;
	val |= (unsigned long)DOVE_SDRAM_BASE & 0xffff0000;
	writel(val, mcbase + DOVE_SDRAM_REGS_BASE_DECODE);

	/* tell the axi controller about where to find the DDR controller */
	val = readl(DOVE_AXI_CTRL) & 0x007fffff;
	val |= (unsigned long)DOVE_SDRAM_BASE & 0xff800000;
	writel(val, DOVE_AXI_CTRL);

	/*
	 * The AXI units internal space base starts at the same address as the
	 * DDR controller.
	 */
	val = readl(DOVE_CPU_CTRL) & 0xfff007ff;
	val |= ((unsigned long)DOVE_SDRAM_BASE & 0xff800000) >> 12;
	writel(val, DOVE_CPU_CTRL);

	barebox_arm_entry(0, dove_memory_find(), boarddata);
}

void __naked __noreturn kirkwood_barebox_entry(void *boarddata)
{
	mvebu_remap_registers();

	barebox_arm_entry(0, kirkwood_memory_find(), boarddata);
}

void __naked __noreturn armada_370_xp_barebox_entry(void *boarddata)
{
	mvebu_remap_registers();

	barebox_arm_entry(0, armada_370_xp_memory_find(), boarddata);
}
