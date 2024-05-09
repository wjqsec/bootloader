/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __MACH_AT91_XLOAD_H
#define __MACH_AT91_XLOAD_H

#include <linux/compiler.h>
#include <pbl/bio.h>

void __noreturn sama5d2_sdhci_start_image(u32 r4);
void __noreturn sama5d3_atmci_start_image(u32 r4, unsigned int clock,
					  unsigned int slot);

int at91_sdhci_bio_init(struct pbl_bio *bio, void __iomem *base);
int at91_mci_bio_init(struct pbl_bio *bio, void __iomem *base,
		      unsigned int clock, unsigned int slot);
void at91_mci_bio_set_highcapacity(bool highcapacity_card);

void __noreturn sam9263_atmci_start_image(u32 mmc_id, unsigned int clock,
					  bool slot_b);


#endif /* __MACH_AT91_XLOAD_H */
