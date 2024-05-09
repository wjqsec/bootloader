// SPDX-License-Identifier: GPL-2.0-only

#include <bootsource.h>
#include <bootstrap.h>
#include <common.h>
#include <malloc.h>
#include <init.h>
#include <envfs.h>
#include <linux/sizes.h>
#include <fs.h>
#include <io.h>

#include <image-metadata.h>

#include <linux/clkdev.h>
#include <linux/stat.h>
#include <linux/clk.h>

#include <mach/socfpga/generic.h>
#include <mach/socfpga/cyclone5-system-manager.h>
#include <mach/socfpga/cyclone5-regs.h>

static struct socfpga_barebox_part default_parts[] = {
	{
		.nor_offset = SZ_256K,
		.nor_size = SZ_1M,
		.mmc_disk = "disk0.1",
	},
	{ /* sentinel */ }
};
const struct socfpga_barebox_part *barebox_parts = default_parts;

static __noreturn int socfpga_xload(void)
{
	enum bootsource bootsource = bootsource_get();
	const struct socfpga_barebox_part *part;
	void *buf = NULL;
	size_t bufsize;

	switch (bootsource) {
	case BOOTSOURCE_MMC:
		socfpga_cyclone5_mmc_init();

		for (part = barebox_parts; part->mmc_disk; part++) {
			buf = bootstrap_read_disk(barebox_parts->mmc_disk, "fat", &bufsize);
			if (!buf) {
				pr_info("failed to load barebox from MMC %s\n",
					part->mmc_disk);
				continue;
			}
		}
		if (!buf) {
			pr_err("failed to load barebox.bin from MMC\n");
			hang();
		}

		if (IS_ENABLED(CONFIG_IMD))
			if (imd_verify_crc32(buf, bufsize) == -EILSEQ) {
				pr_err("failed to verify barebox.bin loaded from eMMC\n");
				hang();
			}

		break;
	case BOOTSOURCE_SPI:
		socfpga_cyclone5_qspi_init();
		for (part = barebox_parts; part->nor_size; part++) {
			buf = bootstrap_read_devfs("mtd0", false,
					part->nor_offset, part->nor_size, SZ_1M, &bufsize);
			if (!buf) {
				pr_info("failed to load barebox from QSPI NOR flash at offset %#x\n",
					part->nor_offset);
				continue;
			}

			if (IS_ENABLED(CONFIG_IMD))
				if (imd_verify_crc32(buf, bufsize) == -EILSEQ) {
					pr_err("failed to verify barebox loaded from "
					       "QSPI NOR flash at offset %#x\n",
					       part->nor_offset);
					free(buf);
					buf = NULL;
					continue;
				}

			break;
		}

		if (!buf) {
			pr_err("failed to load barebox from QSPI NOR flash\n");
			hang();
		}

		break;
	default:
		pr_err("unknown bootsource %d\n", bootsource);
		hang();
	}

	pr_info("starting bootloader...\n");

	bootstrap_boot(buf, 0);

	hang();
}

static int socfpga_devices_init(void)
{
	barebox_set_model("SoCFPGA");
	socfpga_cyclone5_timer_init();
	socfpga_cyclone5_uart_init();

	barebox_main = socfpga_xload;

	return 0;
}
coredevice_initcall(socfpga_devices_init);
