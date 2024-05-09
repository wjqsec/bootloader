/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Freescale Semiconductor, Inc. 2006.
 * Author: Jason Jin<Jason.jin@freescale.com>
 *         Zhang Wei<wei.zhang@freescale.com>
 */
#ifndef _AHCI_H_
#define _AHCI_H_

#define AHCI_PCI_BAR		0x24
#define AHCI_MAX_SG		56 /* hardware max is 64K */
#define AHCI_CMD_SLOT_SZ	32
#define AHCI_MAX_CMDS		32
#define AHCI_CMD_LIST_SZ	(AHCI_CMD_SLOT_SZ * AHCI_MAX_CMDS)
#define AHCI_RX_FIS_SZ		256
#define AHCI_CMD_TBL_HDR_SZ	0x80
#define AHCI_CMD_TBL_CDB	0x40
#define AHCI_CMD_TBL_ITM_SZ	16
#define AHCI_CMD_TBL_SZ		(AHCI_CMD_TBL_HDR_SZ + (AHCI_MAX_SG * AHCI_CMD_TBL_ITM_SZ))
#define AHCI_PORT_PRIV_DMA_SZ	(AHCI_CMD_LIST_SZ + AHCI_CMD_TBL_SZ + AHCI_RX_FIS_SZ)

#define AHCI_CMD_ATAPI		(1 << 5)
#define AHCI_CMD_WRITE		(1 << 6)
#define AHCI_CMD_PREFETCH	(1 << 7)
#define AHCI_CMD_RESET		(1 << 8)
#define AHCI_CMD_CLR_BUSY	(1 << 10)

#define RX_FIS_D2H_REG		0x40	/* offset of D2H Register FIS data */

/* Global controller registers */
#define HOST_CAP		0x00 /* host capabilities */
#define HOST_CTL		0x04 /* global host control */
#define HOST_IRQ_STAT		0x08 /* interrupt status */
#define HOST_PORTS_IMPL		0x0c /* bitmap of implemented ports */
#define HOST_VERSION		0x10 /* AHCI spec. version compliancy */
#define HOST_CAP2		0x24 /* host capabilities, extended */

/* HOST_CAP bits */
#define HOST_CAP_64		(1 << 31)   /* PCI DAC (64-bit DMA) support */
#define HOST_CAP_NCQ		(1 << 30)   /* Native Command Queueing */
#define HOST_CAP_SNTF		(1 << 29)   /* SNotification register */
#define HOST_CAP_SMPS		(1 << 28)   /* Supports mechanical presence switch */
#define HOST_CAP_SSS		(1 << 27)   /* Supports staggered spin-up */
#define HOST_CAP_ALPM		(1 << 26)   /* Aggressive Link PM support */
#define HOST_CAP_LED		(1 << 25)   /* Supports activity LED */
#define HOST_CAP_CLO		(1 << 24)   /* Command List Override support */
#define HOST_CAP_ISS		(0xf << 20) /* Interface Speed Support */
#define HOST_CAP_RESERVED	(1 << 19)   /* Reserved bit */
#define HOST_CAP_ONLY		(1 << 18)   /* Supports AHCI mode only */
#define HOST_CAP_SPM		(1 << 17)   /* Supports port multiplier */
#define HOST_CAP_FBS		(1 << 16)   /* FIS-based switching support */
#define HOST_CAP_PIO_MULTI	(1 << 15)   /* PIO multiple DRQ support */
#define HOST_CAP_SSC		(1 << 14)   /* Slumber state capable */
#define HOST_CAP_PART		(1 << 13)   /* Partial state capable */
#define HOST_CAP_NCS		(0x1f << 8) /* Number of Command Slots */
#define HOST_CAP_CCC		(1 << 7)    /* Command Completion Coalescing */
#define HOST_CAP_EMS		(1 << 6)    /* Enclosure Management support */
#define HOST_CAP_SXS		(1 << 5)    /* Supports External SATA */
#define HOST_CAP_NP		(0x1f << 0) /* Number of ports */

/* HOST_CAP2 bits */
#define HOST_CAP2_APST		(1 << 2)    /* Automatic partial to slumber */
#define HOST_CAP2_NVMHCI	(1 << 1)    /* NVMHCI supported */
#define HOST_CAP2_BOH		(1 << 0)    /* BIOS/OS handoff supported */

/* HOST_CTL bits */
#define HOST_RESET		(1 << 0)  /* reset controller; self-clear */
#define HOST_IRQ_EN		(1 << 1)  /* global IRQ enable */
#define HOST_AHCI_EN		(1 << 31) /* AHCI enabled */

/* Registers for each SATA port */
#define PORT_LST_ADDR		0x00 /* command list DMA addr */
#define PORT_LST_ADDR_HI	0x04 /* command list DMA addr hi */
#define PORT_FIS_ADDR		0x08 /* FIS rx buf addr */
#define PORT_FIS_ADDR_HI	0x0c /* FIS rx buf addr hi */
#define PORT_IRQ_STAT		0x10 /* interrupt status */
#define PORT_IRQ_MASK		0x14 /* interrupt enable/disable mask */
#define PORT_CMD		0x18 /* port command */
#define PORT_TFDATA		0x20 /* taskfile data */
#define PORT_SIG		0x24 /* device TF signature */
#define PORT_CMD_ISSUE		0x38 /* command issue */
#define PORT_SCR		0x28 /* SATA phy register block */
#define PORT_SCR_STAT		0x28 /* SATA phy register: SStatus */
#define PORT_SCR_CTL		0x2c /* SATA phy register: SControl */
#define PORT_SCR_ERR		0x30 /* SATA phy register: SError */
#define PORT_SCR_ACT		0x34 /* SATA phy register: SActive */

/* PORT_IRQ_{STAT,MASK} bits */
#define PORT_IRQ_COLD_PRES	(1 << 31) /* cold presence detect */
#define PORT_IRQ_TF_ERR		(1 << 30) /* task file error */
#define PORT_IRQ_HBUS_ERR	(1 << 29) /* host bus fatal error */
#define PORT_IRQ_HBUS_DATA_ERR	(1 << 28) /* host bus data error */
#define PORT_IRQ_IF_ERR		(1 << 27) /* interface fatal error */
#define PORT_IRQ_IF_NONFATAL	(1 << 26) /* interface non-fatal error */
#define PORT_IRQ_OVERFLOW	(1 << 24) /* xfer exhausted available S/G */
#define PORT_IRQ_BAD_PMP	(1 << 23) /* incorrect port multiplier */

#define PORT_IRQ_PHYRDY		(1 << 22) /* PhyRdy changed */
#define PORT_IRQ_DEV_ILCK	(1 << 7) /* device interlock */
#define PORT_IRQ_CONNECT	(1 << 6) /* port connect change status */
#define PORT_IRQ_SG_DONE	(1 << 5) /* descriptor processed */
#define PORT_IRQ_UNK_FIS	(1 << 4) /* unknown FIS rx'd */
#define PORT_IRQ_SDB_FIS	(1 << 3) /* Set Device Bits FIS rx'd */
#define PORT_IRQ_DMAS_FIS	(1 << 2) /* DMA Setup FIS rx'd */
#define PORT_IRQ_PIOS_FIS	(1 << 1) /* PIO Setup FIS rx'd */
#define PORT_IRQ_D2H_REG_FIS	(1 << 0) /* D2H Register FIS rx'd */

#define PORT_IRQ_FATAL		PORT_IRQ_TF_ERR | PORT_IRQ_HBUS_ERR	\
				| PORT_IRQ_HBUS_DATA_ERR | PORT_IRQ_IF_ERR

#define DEF_PORT_IRQ		PORT_IRQ_FATAL | PORT_IRQ_PHYRDY	\
				| PORT_IRQ_CONNECT | PORT_IRQ_SG_DONE	\
				| PORT_IRQ_UNK_FIS | PORT_IRQ_SDB_FIS	\
				| PORT_IRQ_DMAS_FIS | PORT_IRQ_PIOS_FIS	\
				| PORT_IRQ_D2H_REG_FIS

/* PORT_CMD bits */
#define PORT_CMD_ATAPI		(1 << 24) /* Device is ATAPI */
#define PORT_CMD_LIST_ON	(1 << 15) /* cmd list DMA engine running */
#define PORT_CMD_FIS_ON		(1 << 14) /* FIS DMA engine running */
#define PORT_CMD_FIS_RX		(1 << 4) /* Enable FIS receive DMA engine */
#define PORT_CMD_CLO		(1 << 3) /* Command list override */
#define PORT_CMD_POWER_ON	(1 << 2) /* Power up device */
#define PORT_CMD_SPIN_UP	(1 << 1) /* Spin up device */
#define PORT_CMD_START		(1 << 0) /* Enable port DMA engine */

#define PORT_CMD_ICC_ACTIVE	(0x1 << 28) /* Put i/f in active state */
#define PORT_CMD_ICC_PARTIAL	(0x2 << 28) /* Put i/f in partial state */
#define PORT_CMD_ICC_SLUMBER	(0x6 << 28) /* Put i/f in slumber state */

/* PORT_SCR_STAT bits */
#define PORT_SCR_STAT_DET	(0xf << 0) /* device detection */

#define AHCI_MAX_PORTS		32

/* SETFEATURES stuff */
#define SETFEATURES_XFER	0x03
#define XFER_UDMA_7		0x47
#define XFER_UDMA_6		0x46
#define XFER_UDMA_5		0x45
#define XFER_UDMA_4		0x44
#define XFER_UDMA_3		0x43
#define XFER_UDMA_2		0x42
#define XFER_UDMA_1		0x41
#define XFER_UDMA_0		0x40
#define XFER_MW_DMA_2		0x22
#define XFER_MW_DMA_1		0x21
#define XFER_MW_DMA_0		0x20
#define XFER_SW_DMA_2		0x12
#define XFER_SW_DMA_1		0x11
#define XFER_SW_DMA_0		0x10
#define XFER_PIO_4		0x0C
#define XFER_PIO_3		0x0B
#define XFER_PIO_2		0x0A
#define XFER_PIO_1		0x09
#define XFER_PIO_0		0x08
#define XFER_PIO_SLOW		0x00

#define ATA_FLAG_SATA		(1 << 3)
#define ATA_FLAG_NO_LEGACY	(1 << 4) /* no legacy mode check */
#define ATA_FLAG_MMIO		(1 << 6) /* use MMIO, not PIO */
#define ATA_FLAG_SATA_RESET	(1 << 7) /* (obsolete) use COMRESET */
#define ATA_FLAG_PIO_DMA	(1 << 8) /* PIO cmds via DMA */
#define ATA_FLAG_NO_ATAPI	(1 << 11) /* No ATAPI support */

/* Command list entry DW0 bits */
#define CMD_LIST_OPTS_WRITE	(1 << 6) /* the direction is a device write */

struct ahci_device;

struct ahci_port {
	struct ata_port		ata;
	struct ahci_device	*ahci;
	int			num;
	unsigned		flags;
	void __iomem		*port_mmio;
	struct ahci_cmd_hdr	*cmd_slot;
	dma_addr_t		cmd_slot_dma;
	struct ahci_sg		*cmd_tbl_sg;
	void			*cmd_tbl;
	dma_addr_t		cmd_tbl_dma;
	void			*rx_fis;
	dma_addr_t		rx_fis_dma;
};

struct ahci_device {
	struct device		*dev;
	struct ahci_port	ports[AHCI_MAX_PORTS];
	u32			n_ports;
	void __iomem		*mmio_base;
	u32			cap;		/* cache of HOST_CAP register */
	u32			port_map;	/* cache of HOST_PORTS_IMPL reg */
	u32			pio_mask;
	u32			udma_mask;
	u32			host_flags;
};

int ahci_add_host(struct ahci_device *ahci);
void ahci_print_info(struct ahci_device *ahci);
void ahci_info(struct device *dev);

#endif
