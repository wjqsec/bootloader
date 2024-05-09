// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) "xload-gpmi-nand: " fmt

#include <common.h>
#include <stmp-device.h>
#include <asm-generic/io.h>
#include <linux/sizes.h>
#include <linux/mtd/nand.h>
#include <linux/bitfield.h>
#include <asm/cache.h>
#include <mach/imx/xload.h>
#include <soc/imx/imx-nand-bcb.h>
#include <linux/mtd/rawnand.h>
#include <soc/imx/gpmi-nand.h>
#include <mach/imx/imx6-regs.h>
#include <mach/imx/imx7-regs.h>
#include <mach/imx/clock-imx6.h>
#include <dma/apbh-dma.h>

struct apbh_dma {
	void __iomem *regs;
	enum mxs_dma_id id;
};

struct mxs_dma_chan {
	unsigned int flags;
	int channel;
	struct apbh_dma *apbh;
};

/* udelay() is not available in PBL, need to improvise */
static void __udelay(int us)
{
	volatile int i;

	for (i = 0; i < us * 4; i++);
}

/*
 * Enable a DMA channel.
 *
 * If the given channel has any DMA descriptors on its active list, this
 * function causes the DMA hardware to begin processing them.
 *
 * This function marks the DMA channel as "busy," whether or not there are any
 * descriptors to process.
 */
static int mxs_dma_enable(struct mxs_dma_chan *pchan,
	struct mxs_dma_cmd *pdesc)
{
	struct apbh_dma *apbh = pchan->apbh;
	int channel_bit;
	int channel = pchan->channel;
	unsigned long pdesc32 = (unsigned long)pdesc;

	if (apbh_dma_is_imx23(apbh)) {
		writel(pdesc32,
			apbh->regs + HW_APBHX_CHn_NXTCMDAR_MX23(channel));
		writel(1, apbh->regs + HW_APBHX_CHn_SEMA_MX23(channel));
		channel_bit = channel + BP_APBH_CTRL0_CLKGATE_CHANNEL;
	} else {
		writel(pdesc32,
			apbh->regs + HW_APBHX_CHn_NXTCMDAR_MX28(channel));
		writel(1, apbh->regs + HW_APBHX_CHn_SEMA_MX28(channel));
		channel_bit = channel;
	}

	writel(1 << channel_bit, apbh->regs +
		HW_APBHX_CTRL0 + STMP_OFFSET_REG_CLR);

	return 0;
}

/*
 * Resets the DMA channel hardware.
 */
static int mxs_dma_reset(struct mxs_dma_chan *pchan)
{
	struct apbh_dma *apbh = pchan->apbh;

	if (apbh_dma_is_imx23(apbh))
		writel(1 << (pchan->channel + BP_APBH_CTRL0_RESET_CHANNEL),
			apbh->regs + HW_APBHX_CTRL0 + STMP_OFFSET_REG_SET);
	else
		writel(1 << (pchan->channel +
			BP_APBHX_CHANNEL_CTRL_RESET_CHANNEL),
			apbh->regs + HW_APBHX_CHANNEL_CTRL +
			STMP_OFFSET_REG_SET);

	return 0;
}

static int mxs_dma_wait_complete(struct mxs_dma_chan *pchan)
{
	struct apbh_dma *apbh = pchan->apbh;
	int timeout = 1000000;

	while (1) {
		if (readl(apbh->regs + HW_APBHX_CTRL1) & (1 << pchan->channel))
			return 0;

		if (!timeout--)
			return -ETIMEDOUT;
	}
}

/*
 * Execute the DMA channel
 */
static int mxs_dma_run(struct mxs_dma_chan *pchan, struct mxs_dma_cmd *pdesc,
		int num)
{
	struct apbh_dma *apbh = pchan->apbh;
	int i, ret;

	/* chain descriptors */
	for (i = 0; i < num - 1; i++) {
		pdesc[i].next = (unsigned long)(&pdesc[i + 1]);
		pdesc[i].data |= MXS_DMA_DESC_CHAIN;
	}

	writel(1 << (pchan->channel + BP_APBHX_CTRL1_CH_CMDCMPLT_IRQ_EN),
		apbh->regs + HW_APBHX_CTRL1 + STMP_OFFSET_REG_SET);

	ret = mxs_dma_enable(pchan, pdesc);
	if (ret) {
		pr_err("%s: Failed to enable dma channel: %d\n",
			__func__, ret);
		return ret;
	}

	ret = mxs_dma_wait_complete(pchan);
	if (ret) {
		pr_err("%s: Failed to wait for completion: %d\n",
			__func__, ret);
		return ret;
	}

	/* Shut the DMA channel down. */
	writel(1 << pchan->channel, apbh->regs +
		HW_APBHX_CTRL1 + STMP_OFFSET_REG_CLR);
	writel(1 << pchan->channel, apbh->regs +
		HW_APBHX_CTRL2 + STMP_OFFSET_REG_CLR);

	mxs_dma_reset(pchan);

	writel(1 << (pchan->channel + BP_APBHX_CTRL1_CH_CMDCMPLT_IRQ_EN),
		apbh->regs + HW_APBHX_CTRL1 + STMP_OFFSET_REG_CLR);

	return 0;
}

/* ----------------------------- NAND driver part -------------------------- */

struct mxs_nand_info {
	void __iomem *io_base;
	void __iomem *bch_base;
	struct mxs_dma_chan *dma_channel;
	int cs;
	uint8_t *cmd_buf;
	struct mxs_dma_cmd *desc;
	struct fcb_block fcb;
	int dbbt_num_entries;
	u32 *dbbt;
	struct nand_memory_organization organization;
	unsigned long nand_size;
};

/**
 * It was discovered that xloading barebox from NAND sometimes fails. Observed
 * behaviour is similar to silicon errata ERR007117 for i.MX6.
 *
 * ERR007117 description:
 * For raw NAND boot, ROM switches the source of enfc_clk_root from PLL2_PFD2
 * to PLL3. The root clock is required to be gated before switching the source
 * clock. If the root clock is not gated, clock glitches might be passed to the
 * divider that follows the clock mux, and the divider might behave
 * unpredictably. This can cause the clock generation to fail and the chip will
 * not boot successfully.
 *
 * Workaround solution for this errata:
 * 1) gate all GPMI/BCH related clocks (CG15, G14, CG13, CG12 and CG6)
 * 2) reconfigure clocks
 * 3) ungate all GPMI/BCH related clocks
 *
 */
static inline void imx6_errata_007117_enable(void)
{
	u32 reg;

	/* Gate (disable) the GPMI/BCH clocks in CCM_CCGR4 */
	reg = readl(MXC_CCM_CCGR4);
	reg &= ~(0xFF003000);
	writel(reg, MXC_CCM_CCGR4);

	/**
	 * Gate (disable) the enfc_clk_root before changing the enfc_clk_root
	 * source or dividers by clearing CCM_CCGR2[CG7] to 2'b00. This
	 * disables the iomux_ipt_clk_io_clk.
	 */
	reg = readl(MXC_CCM_CCGR2);
	reg &= ~(0x3 << 14);
	writel(reg, MXC_CCM_CCGR2);

	/* Configure CCM_CS2CDR for the new clock source configuration */
	reg = readl(MXC_CCM_CS2CDR);
	reg &= ~(0x7FF0000);
	writel(reg, MXC_CCM_CS2CDR);
	reg |= 0xF0000;
	writel(reg, MXC_CCM_CS2CDR);

	/**
	 * Enable enfc_clk_root by setting CCM_CCGR2[CG7] to 2'b11. This
	 * enables the iomux_ipt_clk_io_clk.
	 */
	reg = readl(MXC_CCM_CCGR2);
	reg |= 0x3 << 14;
	writel(reg, MXC_CCM_CCGR2);

	/* Ungate (enable) the GPMI/BCH clocks in CCM_CCGR4 */
	reg = readl(MXC_CCM_CCGR4);
	reg |= 0xFF003000;
	writel(reg, MXC_CCM_CCGR4);
}

static uint32_t mxs_nand_aux_status_offset(void)
{
	return (MXS_NAND_METADATA_SIZE + 0x3) & ~0x3;
}

static int mxs_nand_read_page(struct mxs_nand_info *info, int writesize,
		int oobsize, int pagenum, void *databuf, int raw, bool randomizer)
{
	void __iomem *bch_regs = info->bch_base;
	unsigned column = 0;
	struct mxs_dma_cmd *d;
	int cmd_queue_len;
	u8 *cmd_buf;
	int ret;
	int timeout;
	int descnum = 0;
	int max_pagenum = info->nand_size /
		info->organization.pagesize;

	memset(info->desc, 0,
		sizeof(*info->desc) * MXS_NAND_DMA_DESCRIPTOR_COUNT);

	/* Compile DMA descriptor - read0 */
	cmd_buf = info->cmd_buf;
	cmd_queue_len = 0;
	d = &info->desc[descnum++];
	d->address = (dma_addr_t)(cmd_buf);
	cmd_buf[cmd_queue_len++] = NAND_CMD_READ0;
	cmd_buf[cmd_queue_len++] = column;
	cmd_buf[cmd_queue_len++] = column >> 8;
	cmd_buf[cmd_queue_len++] = pagenum;
	cmd_buf[cmd_queue_len++] = pagenum >> 8;

	if ((max_pagenum - 1) >= SZ_64K)
		cmd_buf[cmd_queue_len++] = pagenum >> 16;

	d->data = MXS_DMA_DESC_COMMAND_DMA_READ |
		MXS_DMA_DESC_WAIT4END |
		MXS_DMA_DESC_PIO_WORDS(1) |
		MXS_DMA_DESC_XFER_COUNT(cmd_queue_len);

	d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_WRITE |
		GPMI_CTRL0_WORD_LENGTH |
		FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
		GPMI_CTRL0_ADDRESS_NAND_CLE |
		GPMI_CTRL0_ADDRESS_INCREMENT |
		cmd_queue_len;

	/* Compile DMA descriptor - readstart */
	cmd_buf = &info->cmd_buf[8];
	cmd_queue_len = 0;
	d = &info->desc[descnum++];
	d->address = (dma_addr_t)(cmd_buf);

	cmd_buf[cmd_queue_len++] = NAND_CMD_READSTART;

	d->data = MXS_DMA_DESC_COMMAND_DMA_READ |
		MXS_DMA_DESC_WAIT4END |
		MXS_DMA_DESC_PIO_WORDS(1) |
		MXS_DMA_DESC_XFER_COUNT(cmd_queue_len);

	d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_WRITE |
		GPMI_CTRL0_WORD_LENGTH |
		FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
		GPMI_CTRL0_ADDRESS_NAND_CLE |
		GPMI_CTRL0_ADDRESS_INCREMENT |
		cmd_queue_len;

	/* Compile DMA descriptor - wait for ready. */
	d = &info->desc[descnum++];
	d->data = MXS_DMA_DESC_CHAIN |
		MXS_DMA_DESC_NAND_WAIT_4_READY |
		MXS_DMA_DESC_WAIT4END |
		MXS_DMA_DESC_PIO_WORDS(2);

	d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_WAIT_FOR_READY |
		GPMI_CTRL0_WORD_LENGTH |
		FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
		GPMI_CTRL0_ADDRESS_NAND_DATA;

	if (raw) {
		/* Compile DMA descriptor - read. */
		d = &info->desc[descnum++];
		d->data = MXS_DMA_DESC_WAIT4END |
			MXS_DMA_DESC_PIO_WORDS(1) |
			MXS_DMA_DESC_XFER_COUNT(writesize + oobsize) |
			MXS_DMA_DESC_COMMAND_DMA_WRITE;
		d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_READ |
			GPMI_CTRL0_WORD_LENGTH |
			FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
			GPMI_CTRL0_ADDRESS_NAND_DATA |
			(writesize + oobsize);
		d->address = (dma_addr_t)databuf;
	} else {
		/* Compile DMA descriptor - enable the BCH block and read. */
		d = &info->desc[descnum++];
		d->data = MXS_DMA_DESC_WAIT4END | MXS_DMA_DESC_PIO_WORDS(6);
		d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_READ |
			GPMI_CTRL0_WORD_LENGTH |
			FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
			GPMI_CTRL0_ADDRESS_NAND_DATA |
			(writesize + oobsize);
		d->pio_words[1] = 0;
		d->pio_words[2] = GPMI_ECCCTRL_ENABLE_ECC |
			GPMI_ECCCTRL_ECC_CMD_DECODE |
			GPMI_ECCCTRL_BUFFER_MASK_BCH_PAGE;
		d->pio_words[3] = writesize + oobsize;
		d->pio_words[4] = (dma_addr_t)databuf;
		d->pio_words[5] = (dma_addr_t)(databuf + writesize);

		if (randomizer) {
			d->pio_words[2] |= GPMI_ECCCTRL_RANDOMIZER_ENABLE |
					   GPMI_ECCCTRL_RANDOMIZER_TYPE2;
			d->pio_words[3] |= (pagenum % 256) << 16;
		}

		/* Compile DMA descriptor - disable the BCH block. */
		d = &info->desc[descnum++];
		d->data = MXS_DMA_DESC_NAND_WAIT_4_READY |
			MXS_DMA_DESC_WAIT4END |
			MXS_DMA_DESC_PIO_WORDS(3);
		d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_WAIT_FOR_READY |
			GPMI_CTRL0_WORD_LENGTH |
			FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
			GPMI_CTRL0_ADDRESS_NAND_DATA |
			(writesize + oobsize);
	}

	/* Compile DMA descriptor - de-assert the NAND lock and interrupt. */
	d = &info->desc[descnum++];
	d->data = MXS_DMA_DESC_IRQ | MXS_DMA_DESC_DEC_SEM;

	/* Execute the DMA chain. */
	ret = mxs_dma_run(info->dma_channel, info->desc, descnum);
	if (ret) {
		pr_err("DMA read error\n");
		goto err;
	}

	if (raw)
		return 0;

	timeout = 1000000;

	while (1) {
		if (!timeout--) {
			ret = -ETIMEDOUT;
			goto err;
		}

		if (readl(bch_regs + BCH_CTRL) & BCH_CTRL_COMPLETE_IRQ)
			break;
	}

	writel(BCH_CTRL_COMPLETE_IRQ,
		bch_regs + BCH_CTRL + STMP_OFFSET_REG_CLR);

	ret = 0;
err:
	return ret;
}

static int mxs_nand_get_ecc_status(struct mxs_nand_info *info, void *databuf)
{
	uint8_t	*status;
	int i;

	/* Loop over status bytes, accumulating ECC status. */
	status = databuf + info->organization.pagesize + mxs_nand_aux_status_offset();
	for (i = 0; i < info->organization.pagesize / MXS_NAND_CHUNK_DATA_CHUNK_SIZE; i++) {
		if (status[i] == 0xfe)
			return -EBADMSG;
	}

	return 0;
}

static int mxs_nand_get_read_status(struct mxs_nand_info *info, void *databuf)
{
	int ret;
	u8 *cmd_buf;
	struct mxs_dma_cmd *d;
	int descnum = 0;
	int cmd_queue_len;

	memset(info->desc, 0,
		sizeof(*info->desc) * MXS_NAND_DMA_DESCRIPTOR_COUNT);

	/* Compile DMA descriptor - READ STATUS */
	cmd_buf = info->cmd_buf;
	cmd_queue_len = 0;
	d = &info->desc[descnum++];
	d->address = (dma_addr_t)(cmd_buf);
	cmd_buf[cmd_queue_len++] = NAND_CMD_STATUS;

	d->data = MXS_DMA_DESC_COMMAND_DMA_READ |
		MXS_DMA_DESC_WAIT4END |
		MXS_DMA_DESC_PIO_WORDS(1) |
		MXS_DMA_DESC_XFER_COUNT(cmd_queue_len);

	d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_WRITE |
		GPMI_CTRL0_WORD_LENGTH |
		FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
		GPMI_CTRL0_ADDRESS_NAND_CLE |
		GPMI_CTRL0_ADDRESS_INCREMENT |
		cmd_queue_len;

	/* Compile DMA descriptor - read. */
	d = &info->desc[descnum++];
	d->data = MXS_DMA_DESC_WAIT4END |
		MXS_DMA_DESC_PIO_WORDS(1) |
		MXS_DMA_DESC_XFER_COUNT(1) |
		MXS_DMA_DESC_COMMAND_DMA_WRITE;
	d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_READ |
		GPMI_CTRL0_WORD_LENGTH |
		FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
		GPMI_CTRL0_ADDRESS_NAND_DATA |
		(1);
	d->address = (dma_addr_t)databuf;

	/* Compile DMA descriptor - de-assert the NAND lock and interrupt. */
	d = &info->desc[descnum++];
	d->data = MXS_DMA_DESC_IRQ | MXS_DMA_DESC_DEC_SEM;

	/* Execute the DMA chain. */
	ret = mxs_dma_run(info->dma_channel, info->desc, descnum);
	if (ret) {
		pr_err("DMA read error\n");
		return ret;
	}

	return ret;
}

static int mxs_nand_reset(struct mxs_nand_info *info, void *databuf)
{
	int ret, i;
	u8 *cmd_buf;
	struct mxs_dma_cmd *d;
	int descnum = 0;
	int cmd_queue_len;
	u8 read_status;

	memset(info->desc, 0,
		sizeof(*info->desc) * MXS_NAND_DMA_DESCRIPTOR_COUNT);

	/* Compile DMA descriptor - RESET */
	cmd_buf = info->cmd_buf;
	cmd_queue_len = 0;
	d = &info->desc[descnum++];
	d->address = (dma_addr_t)(cmd_buf);
	cmd_buf[cmd_queue_len++] = NAND_CMD_RESET;

	d->data = MXS_DMA_DESC_COMMAND_DMA_READ |
		MXS_DMA_DESC_WAIT4END |
		MXS_DMA_DESC_PIO_WORDS(1) |
		MXS_DMA_DESC_XFER_COUNT(cmd_queue_len);

	d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_WRITE |
		GPMI_CTRL0_WORD_LENGTH |
		FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
		GPMI_CTRL0_ADDRESS_NAND_CLE |
		GPMI_CTRL0_ADDRESS_INCREMENT |
		cmd_queue_len;

	/* Compile DMA descriptor - de-assert the NAND lock and interrupt. */
	d = &info->desc[descnum++];
	d->data = MXS_DMA_DESC_IRQ | MXS_DMA_DESC_DEC_SEM;

	/* Execute the DMA chain. */
	ret = mxs_dma_run(info->dma_channel, info->desc, descnum);
	if (ret) {
		pr_err("DMA read error\n");
		return ret;
	}

	/* Wait for NAND to wake up */
	for (i = 0; i < 10; i++) {
		__udelay(50000);
		ret = mxs_nand_get_read_status(info, databuf);
		if (ret)
			return ret;
		memcpy(&read_status, databuf, 1);
		if (read_status & NAND_STATUS_READY)
			return 0;
	}

	pr_warn("NAND Reset failed\n");
	return -1;
}

/* function taken from nand_onfi.c */
static u16 onfi_crc16(u16 crc, u8 const *p, size_t len)
{
	int i;

	while (len--) {
		crc ^= *p++ << 8;
		for (i = 0; i < 8; i++)
			crc = (crc << 1) ^ ((crc & 0x8000) ? 0x8005 : 0);
	}
	return crc;
}

static int mxs_nand_get_onfi(struct mxs_nand_info *info, void *databuf)
{
	int ret;
	u8 *cmd_buf;
	u16 crc;
	struct mxs_dma_cmd *d;
	int descnum = 0;
	int cmd_queue_len;
	struct nand_onfi_params nand_params;

	memset(info->desc, 0,
		sizeof(*info->desc) * MXS_NAND_DMA_DESCRIPTOR_COUNT);

	/* Compile DMA descriptor - READ PARAMETER PAGE */
	cmd_buf = info->cmd_buf;
	cmd_queue_len = 0;
	d = &info->desc[descnum++];
	d->address = (dma_addr_t)(cmd_buf);
	cmd_buf[cmd_queue_len++] = NAND_CMD_PARAM;
	cmd_buf[cmd_queue_len++] = 0x00;

	d->data = MXS_DMA_DESC_COMMAND_DMA_READ |
		MXS_DMA_DESC_WAIT4END |
		MXS_DMA_DESC_PIO_WORDS(1) |
		MXS_DMA_DESC_XFER_COUNT(cmd_queue_len);

	d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_WRITE |
		GPMI_CTRL0_WORD_LENGTH |
		FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
		GPMI_CTRL0_ADDRESS_NAND_CLE |
		GPMI_CTRL0_ADDRESS_INCREMENT |
		cmd_queue_len;

	/* Compile DMA descriptor - wait for ready. */
	d = &info->desc[descnum++];
	d->data = MXS_DMA_DESC_CHAIN |
		MXS_DMA_DESC_NAND_WAIT_4_READY |
		MXS_DMA_DESC_WAIT4END |
		MXS_DMA_DESC_PIO_WORDS(2);

	d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_WAIT_FOR_READY |
		GPMI_CTRL0_WORD_LENGTH |
		FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
		GPMI_CTRL0_ADDRESS_NAND_DATA;

	/* Compile DMA descriptor - read. */
	d = &info->desc[descnum++];
	d->data = MXS_DMA_DESC_WAIT4END |
		MXS_DMA_DESC_PIO_WORDS(1) |
		MXS_DMA_DESC_XFER_COUNT(sizeof(struct nand_onfi_params)) |
		MXS_DMA_DESC_COMMAND_DMA_WRITE;
	d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_READ |
		GPMI_CTRL0_WORD_LENGTH |
		FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
		GPMI_CTRL0_ADDRESS_NAND_DATA |
		(sizeof(struct nand_onfi_params));
	d->address = (dma_addr_t)databuf;

	/* Compile DMA descriptor - de-assert the NAND lock and interrupt. */
	d = &info->desc[descnum++];
	d->data = MXS_DMA_DESC_IRQ | MXS_DMA_DESC_DEC_SEM;

	/* Execute the DMA chain. */
	ret = mxs_dma_run(info->dma_channel, info->desc, descnum);
	if (ret) {
		pr_err("DMA read error\n");
		return ret;
	}

	memcpy(&nand_params, databuf, sizeof(struct nand_onfi_params));

	crc = onfi_crc16(NAND_ONFI_CRC_BASE, (u8 *)&nand_params, 254);
	pr_debug("ONFI CRC: 0x%x, CALC. CRC 0x%x\n", nand_params.crc, crc);
	if (crc != le16_to_cpu(nand_params.crc)) {
		pr_debug("ONFI CRC mismatch!\n");
		ret = -EUCLEAN;
		return ret;
	}

	/* Fill the NAND organization struct with data */
	info->organization.bits_per_cell = nand_params.bits_per_cell;
	info->organization.pagesize = le32_to_cpu(nand_params.byte_per_page);
	info->organization.oobsize =
		le16_to_cpu(nand_params.spare_bytes_per_page);
	info->organization.pages_per_eraseblock =
		le32_to_cpu(nand_params.pages_per_block);
	info->organization.eraseblocks_per_lun =
		le32_to_cpu(nand_params.blocks_per_lun);
	info->organization.max_bad_eraseblocks_per_lun =
		le16_to_cpu(nand_params.bb_per_lun);
	info->organization.luns_per_target = nand_params.lun_count;
	info->nand_size = info->organization.pagesize *
		info->organization.pages_per_eraseblock *
		info->organization.eraseblocks_per_lun *
		info->organization.luns_per_target;

	return ret;
}

static int mxs_nand_read_id(struct mxs_nand_info *info, u8 adr, void *databuf, size_t len)
{
	int ret;
	u8 *cmd_buf;
	struct mxs_dma_cmd *d;
	int descnum = 0;
	int cmd_queue_len;

	memset(info->desc, 0,
		sizeof(*info->desc) * MXS_NAND_DMA_DESCRIPTOR_COUNT);

	/* Compile DMA descriptor - READID + 0x20 (ADDR) */
	cmd_buf = info->cmd_buf;
	cmd_queue_len = 0;
	d = &info->desc[descnum++];
	d->address = (dma_addr_t)(cmd_buf);
	cmd_buf[cmd_queue_len++] = NAND_CMD_READID;
	cmd_buf[cmd_queue_len++] = adr;

	d->data = MXS_DMA_DESC_COMMAND_DMA_READ |
		MXS_DMA_DESC_WAIT4END |
		MXS_DMA_DESC_PIO_WORDS(1) |
		MXS_DMA_DESC_XFER_COUNT(cmd_queue_len);

	d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_WRITE |
		GPMI_CTRL0_WORD_LENGTH |
		FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
		GPMI_CTRL0_ADDRESS_NAND_CLE |
		GPMI_CTRL0_ADDRESS_INCREMENT |
		cmd_queue_len;

	/* Compile DMA descriptor - read. */
	d = &info->desc[descnum++];
	d->data = MXS_DMA_DESC_WAIT4END |
		MXS_DMA_DESC_PIO_WORDS(1) |
		MXS_DMA_DESC_XFER_COUNT(len) |
		MXS_DMA_DESC_COMMAND_DMA_WRITE;
	d->pio_words[0] = GPMI_CTRL0_COMMAND_MODE_READ |
		GPMI_CTRL0_WORD_LENGTH |
		FIELD_PREP(GPMI_CTRL0_CS, info->cs) |
		GPMI_CTRL0_ADDRESS_NAND_DATA |
		len;
	d->address = (dma_addr_t)databuf;

	/* Compile DMA descriptor - de-assert the NAND lock and interrupt. */
	d = &info->desc[descnum++];
	d->data = MXS_DMA_DESC_IRQ | MXS_DMA_DESC_DEC_SEM;

	/* Execute the DMA chain. */
	ret = mxs_dma_run(info->dma_channel, info->desc, descnum);
	if (ret)
		pr_err("DMA read error\n");

	return ret;
}

struct onfi_header {
	u8 byte0;
	u8 byte1;
	u8 byte2;
	u8 byte3;
};

static int mxs_nand_check_onfi(struct mxs_nand_info *info, void *databuf)
{
	int ret;
	struct onfi_header *onfi_head = databuf;

	ret = mxs_nand_read_id(info, 0x20, databuf, sizeof(struct onfi_header));
	if (ret)
		return ret;

	pr_debug("ONFI Byte0: 0x%x\n", onfi_head->byte0);
	pr_debug("ONFI Byte1: 0x%x\n", onfi_head->byte1);
	pr_debug("ONFI Byte2: 0x%x\n", onfi_head->byte2);
	pr_debug("ONFI Byte3: 0x%x\n", onfi_head->byte3);

	if (onfi_head->byte0 != 'O' || onfi_head->byte1 != 'N' ||
		onfi_head->byte2 != 'F' || onfi_head->byte3 != 'I')
		return 1;

	return 0;
}

struct readid_data {
	u8 byte0;
	u8 byte1;
	u8 byte2;
	u8 byte3;
	u8 byte4;
};

static int mxs_nand_get_readid(struct mxs_nand_info *info, void *databuf)
{
	int ret;
	struct readid_data *id_data = databuf;

	ret = mxs_nand_read_id(info, 0x0, databuf, sizeof(struct readid_data));
	if (ret)
		return ret;

	pr_debug("NAND Byte0: 0x%x\n", id_data->byte0);
	pr_debug("NAND Byte1: 0x%x\n", id_data->byte1);
	pr_debug("NAND Byte2: 0x%x\n", id_data->byte2);
	pr_debug("NAND Byte3: 0x%x\n", id_data->byte3);
	pr_debug("NAND Byte4: 0x%x\n", id_data->byte4);

	if (id_data->byte0 == 0xff || id_data->byte1 == 0xff ||
		id_data->byte2 == 0xff || id_data->byte3 == 0xff ||
		id_data->byte4 == 0xff) {
		pr_err("\"READ ID\" returned 0xff, possible error!\n");
		return -EOVERFLOW;
	}

	/* Fill the NAND organization struct with data */
	info->organization.bits_per_cell =
		(1 << ((id_data->byte2 >> 2) & 0x3)) * 2;
	info->organization.pagesize =
		(1 << (id_data->byte3 & 0x3)) * SZ_1K;
	info->organization.oobsize = id_data->byte3 & 0x4 ?
		info->organization.pagesize / 512 * 16 :
		info->organization.pagesize / 512 * 8;
	info->organization.pages_per_eraseblock =
		(1 << ((id_data->byte3 >> 4) & 0x3)) * SZ_64K /
		info->organization.pagesize;
	info->organization.planes_per_lun =
		1 << ((id_data->byte4 >> 2) & 0x3);
	info->nand_size = info->organization.planes_per_lun *
		(1 << ((id_data->byte4 >> 4) & 0x7)) * SZ_8M;
	info->organization.eraseblocks_per_lun = info->nand_size /
		(info->organization.pages_per_eraseblock *
		info->organization.pagesize);

	return ret;
}

static int mxs_nand_get_info(struct mxs_nand_info *info, void *databuf)
{
	int ret, i;

	ret = mxs_nand_reset(info, databuf);
	if (ret)
		return ret;

	ret = mxs_nand_check_onfi(info, databuf);
	if (ret) {
		if (ret != 1)
			return ret;
		pr_info("ONFI not supported, try \"READ ID\"...\n");
	} else {
		/*
		 * Some NAND's don't return the correct data the first time
		 * "READ PARAMETER PAGE" is returned. Execute the command
		 * multimple times
		 */
		for (i = 0; i < 3; i++) {
			/*
			 * Some NAND's need to be reset before "READ PARAMETER
			 * PAGE" can be successfully executed.
			 */
			ret = mxs_nand_reset(info, databuf);
			if (ret)
				return ret;
			ret = mxs_nand_get_onfi(info, databuf);
			if (ret)
				pr_err("ONFI error: %d\n", ret);
			else
				break;
		}
		if (!ret)
			goto succ;
	}

	/*
	 * If ONFI is not supported or if it fails try to get NAND's info from
	 * "READ ID" command.
	 */
	ret = mxs_nand_reset(info, databuf);
	if (ret)
		return ret;
	pr_debug("Trying \"READ ID\" command...\n");
	ret = mxs_nand_get_readid(info, databuf);
	if (ret) {
		pr_err("xloader supports only ONFI and generic \"READ ID\" " \
			"supported NANDs\n");
		return -1;
	}
succ:
	pr_debug("NAND page_size: %d\n", info->organization.pagesize);
	pr_debug("NAND block_size: %d\n",
		info->organization.pages_per_eraseblock
		* info->organization.pagesize);
	pr_debug("NAND oob_size: %d\n", info->organization.oobsize);
	pr_debug("NAND nand_size: %lu\n", info->nand_size);
	pr_debug("NAND bits_per_cell: %d\n", info->organization.bits_per_cell);
	pr_debug("NAND planes_per_lun: %d\n",
		info->organization.planes_per_lun);
	pr_debug("NAND luns_per_target: %d\n",
		info->organization.luns_per_target);
	pr_debug("NAND eraseblocks_per_lun: %d\n",
		info->organization.eraseblocks_per_lun);
	pr_debug("NAND ntargets: %d\n", info->organization.ntargets);


	return 0;
}

/* ---------------------------- BCB handling part -------------------------- */

static uint32_t calc_chksum(void *buf, size_t size)
{
	u32 chksum = 0;
	u8 *bp = buf;
	size_t i;

	for (i = 0; i < size; i++)
		chksum += bp[i];

	return ~chksum;
}

static int imx6_get_fcb(struct mxs_nand_info *info, void *databuf)
{
	int i, pagenum, ret;
	uint32_t checksum;
	struct fcb_block *fcb = &info->fcb;

	/* First page read fails, this shouldn't be necessary */
	mxs_nand_read_page(info, info->organization.pagesize,
		info->organization.oobsize, 0, databuf, 1, false);

	for (i = 0; i < 4; i++) {
		pagenum = info->organization.pages_per_eraseblock * i;

		ret = mxs_nand_read_page(info, info->organization.pagesize,
			info->organization.oobsize, pagenum, databuf, 1, false);
		if (ret)
			continue;

		ret = mxs_nand_get_ecc_status(info, databuf);
		if (ret)
			continue;

		memcpy(fcb, databuf + mxs_nand_aux_status_offset(),
			sizeof(*fcb));

		if (fcb->FingerPrint != FCB_FINGERPRINT) {
			pr_err("No FCB found on page %d\n", pagenum);
			continue;
		}

		checksum = calc_chksum((void *)fcb +
			sizeof(uint32_t), sizeof(*fcb) - sizeof(uint32_t));

		if (checksum != fcb->Checksum) {
			pr_err("FCB on page %d has invalid checksum. " \
				"Expected: 0x%08x, calculated: 0x%08x",
				pagenum, fcb->Checksum, checksum);
			continue;
		}

		return 0;
	}

	return -EINVAL;
}

static int imx7_get_fcb_n(struct mxs_nand_info *info, void *databuf, int num)
{
	int ret;
	int flips = 0;
	uint8_t	*status;
	int i;

	ret = mxs_nand_read_page(info, BCH62_WRITESIZE, BCH62_OOBSIZE,
				 info->organization.pages_per_eraseblock * num, databuf, 0, true);
	if (ret)
		return ret;

	/* Loop over status bytes, accumulating ECC status. */
	status = databuf + BCH62_WRITESIZE + 32;

	for (i = 0; i < 8; i++) {
		switch (status[i]) {
		case 0x0:
			break;
		case 0xff:
			/*
			 * A status of 0xff means the chunk is erased, but due to
			 * the randomizer we see this as random data. Explicitly
			 * memset it.
			 */
			memset(databuf + 0x80 * i, 0xff, 0x80);
			break;
		case 0xfe:
			return -EBADMSG;
		default:
			flips += status[0];
			break;
		}
	}

	return ret;
}

static int imx7_get_fcb(struct mxs_nand_info *info, void *databuf)
{
	int i, ret;
	struct fcb_block *fcb = &info->fcb;

	mxs_nand_mode_fcb_62bit(info->bch_base);

	for (i = 0; i < 4; i++) {
		ret = imx7_get_fcb_n(info, databuf, i);
		if (!ret)
			break;
	}

	if (ret) {
		pr_err("Cannot find FCB\n");
	} else {
		memcpy(fcb, databuf, sizeof(*fcb));
	}

	return ret;
}

static int get_dbbt(struct mxs_nand_info *info, void *databuf)
{
	int i, ret;
	int page;
	int startpage = info->fcb.DBBTSearchAreaStartAddress;
	struct dbbt_block dbbt;

	for (i = 0; i < 4; i++) {
		page = startpage + i * info->organization.pages_per_eraseblock;

		ret = mxs_nand_read_page(info, info->organization.pagesize,
			info->organization.oobsize, page, databuf, 0, false);
		if (ret)
			continue;

		ret = mxs_nand_get_ecc_status(info, databuf);
		if (ret)
			continue;

		memcpy(&dbbt, databuf, sizeof(struct dbbt_block));

		if (*(u32 *)(databuf + sizeof(u32)) != DBBT_FINGERPRINT)
			continue;

		/* Version check */
		if (be32_to_cpup(databuf + 2 * sizeof(u32)) < 1)
			return -ENOENT;

		ret = mxs_nand_read_page(info, info->organization.pagesize,
			info->organization.oobsize, page + 4, databuf, 0, false);
		if (ret)
			continue;

		ret = mxs_nand_get_ecc_status(info, databuf);
		if (ret)
			continue;

		info->dbbt_num_entries = *(u32 *)(databuf + sizeof(u32));

		pr_debug("Found DBBT with %d entries\n",
			info->dbbt_num_entries);
		pr_debug("Checksum = 0x%08x\n", dbbt.Checksum);
		pr_debug("FingerPrint = 0x%08x\n", dbbt.FingerPrint);
		pr_debug("Version = 0x%08x\n", dbbt.Version);
		pr_debug("numberBB = 0x%08x\n", dbbt.numberBB);
		pr_debug("DBBTNumOfPages= 0x%08x\n", dbbt.DBBTNumOfPages);

		for (i = 0; i < info->dbbt_num_entries; i++)
			pr_debug("badblock %d at block %d\n", i,
				*(u32 *)(databuf + (2 + i) * sizeof(u32)));

		info->dbbt = databuf + 2 * sizeof(u32);

		return 0;
	}

	return -ENOENT;
}

static int block_is_bad(struct mxs_nand_info *info, int blocknum)
{
	int i;
	u32 *dbbt = info->dbbt;

	if (!dbbt)
		return 0;

	for (i = 0; i < info->dbbt_num_entries; i++) {
		if (dbbt[i] == blocknum)
			return 1;
	}

	return 0;
}

static int read_firmware(struct mxs_nand_info *info, int startpage,
	void *dest, int len)
{
	int curpage = startpage;
	struct fcb_block *fcb = &info->fcb;
	int pagesperblock = fcb->SectorsPerBlock;
	int numpages = (len / fcb->PageDataSize) + 1;
	int ret;
	int pagesize = fcb->PageDataSize;
	int oobsize = fcb->TotalPageSize - pagesize;

	pr_debug("Reading %d pages starting from page %d\n",
		numpages, startpage);

	if (block_is_bad(info, curpage / pagesperblock))
		curpage = ALIGN_DOWN(curpage + pagesperblock, pagesperblock);

	while (numpages) {
		if (!(curpage & (pagesperblock - 1))) {
			/* Check for bad blocks on each block boundary */
			if (block_is_bad(info, curpage / pagesperblock)) {
				pr_debug("Skipping bad block at page %d\n",
					curpage);
				curpage += pagesperblock;
				continue;
			}
		}

		ret = mxs_nand_read_page(info, pagesize, oobsize,
			curpage, dest, 0, false);
		if (ret) {
			pr_debug("Failed to read page %d\n", curpage);
			return ret;
		}

		*((u8 *)dest + fcb->BadBlockMarkerByte) =
			*(u8 *)(dest + pagesize);

		numpages--;
		dest += pagesize;
		curpage++;
	}

	return 0;
}

struct imx_nand_params {
	struct mxs_nand_info info;
	struct apbh_dma apbh;
	void *sdram;
	int (*get_fcb)(struct mxs_nand_info *info, void *databuf);
};

static int __maybe_unused imx6_nand_load_image(struct imx_nand_params *params,
					       void *databuf, void *dest, int len)
{
	struct mxs_nand_info *info = &params->info;
	struct mxs_dma_chan pchan = {
		.channel = 0, /* MXS: MXS_DMA_CHANNEL_AHB_APBH_GPMI0 */
		.apbh = &params->apbh,
	};
	int ret;
	struct fcb_block *fcb;
	void __iomem *bch_regs = info->bch_base;
	u32 fl0, fl1;

	info->dma_channel = &pchan;

	pr_debug("cmdbuf: 0x%p descs: 0x%p databuf: 0x%p dest: 0x%p\n",
			info->cmd_buf, info->desc, databuf, dest);

	ret = mxs_nand_get_info(info, databuf);
	if (ret)
		return ret;

	ret = params->get_fcb(info, databuf);
	if (ret)
		return ret;

	fcb = &info->fcb;

	pr_debug("Found FCB:\n");
	pr_debug("PageDataSize:     0x%08x\n", fcb->PageDataSize);
	pr_debug("TotalPageSize:    0x%08x\n", fcb->TotalPageSize);
	pr_debug("SectorsPerBlock:  0x%08x\n", fcb->SectorsPerBlock);
	pr_debug("FW1_startingPage: 0x%08x\n",
		fcb->Firmware1_startingPage);
	pr_debug("PagesInFW1:       0x%08x\n", fcb->PagesInFirmware1);
	pr_debug("FW2_startingPage: 0x%08x\n",
		fcb->Firmware2_startingPage);
	pr_debug("PagesInFW2:       0x%08x\n", fcb->PagesInFirmware2);

	info->organization.oobsize = fcb->TotalPageSize - fcb->PageDataSize;
	info->organization.pagesize = fcb->PageDataSize;

	fl0 = FIELD_PREP(BCH_FLASHLAYOUT0_NBLOCKS, fcb->NumEccBlocksPerPage) |
		FIELD_PREP(BCH_FLASHLAYOUT0_META_SIZE, fcb->MetadataBytes) |
		FIELD_PREP(IMX6_BCH_FLASHLAYOUT0_ECC0, fcb->EccBlock0EccType) |
		(fcb->BCHType ? BCH_FLASHLAYOUT0_GF13_0_GF14_1 : 0) |
		FIELD_PREP(BCH_FLASHLAYOUT0_DATA0_SIZE, fcb->EccBlock0Size / 4);
	fl1 = FIELD_PREP(BCH_FLASHLAYOUT1_PAGE_SIZE, fcb->TotalPageSize) |
		FIELD_PREP(IMX6_BCH_FLASHLAYOUT1_ECCN, fcb->EccBlockNEccType) |
		(fcb->BCHType ? BCH_FLASHLAYOUT1_GF13_0_GF14_1 : 0) |
		FIELD_PREP(BCH_FLASHLAYOUT1_DATAN_SIZE, fcb->EccBlockNSize / 4);
	writel(fl0, bch_regs + BCH_FLASH0LAYOUT0);
	writel(fl1, bch_regs + BCH_FLASH0LAYOUT1);

	get_dbbt(info, databuf);

	ret = read_firmware(info, fcb->Firmware1_startingPage, dest, len);
	if (ret) {
		pr_err("Failed to read firmware1, trying firmware2\n");
		ret = read_firmware(info, fcb->Firmware2_startingPage,
			dest, len);
		if (ret) {
			pr_err("Failed to also read firmware2\n");
			return ret;
		}
	}

	return 0;
}

static int imx_nand_start_image(struct imx_nand_params *params)
{
	struct mxs_nand_info *info = &params->info;
	int ret;
	void __noreturn (*bb)(void);
	void *databuf;

	/* Command buffers */
	info->cmd_buf = params->sdram;
	info->desc = params->sdram + MXS_NAND_COMMAND_BUFFER_SIZE;
	databuf = info->desc +
		sizeof(struct mxs_dma_cmd) * MXS_NAND_DMA_DESCRIPTOR_COUNT;
	bb = (void *)PAGE_ALIGN((unsigned long)databuf + SZ_8K);

	ret = imx6_nand_load_image(params, databuf, bb, imx_image_size());
	if (ret) {
		pr_err("Loading image failed: %d\n", ret);
		return ret;
	}

	pr_debug("Starting barebox image at 0x%p\n", bb);

	arm_early_mmu_cache_invalidate();
	barrier();

	bb();
}

int imx6_nand_start_image(void)
{
	static struct imx_nand_params params = {
		.info.io_base = IOMEM(MX6_GPMI_BASE_ADDR),
		.info.bch_base = IOMEM(MX6_BCH_BASE_ADDR),
		.apbh.regs = IOMEM(MX6_APBH_BASE_ADDR),
		.apbh.id = IMX28_DMA,
		.sdram = (void *)MX6_MMDC_PORT01_BASE_ADDR,
		.get_fcb = imx6_get_fcb,
	};

	/* Apply ERR007117 workaround */
	imx6_errata_007117_enable();

	return imx_nand_start_image(&params);
}

int imx7_nand_start_image(void)
{
	static struct imx_nand_params params = {
		.info.io_base = IOMEM(MX7_GPMI_BASE),
		.info.bch_base = IOMEM(MX7_BCH_BASE),
		.apbh.regs = IOMEM(MX7_APBH_BASE),
		.apbh.id = IMX28_DMA,
		.sdram = (void *)MX7_DDR_BASE_ADDR,
		.get_fcb = imx7_get_fcb,
	};

	return imx_nand_start_image(&params);
}
