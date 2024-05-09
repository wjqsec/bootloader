/*
 * $Id: mtd.h,v 1.61 2005/11/07 11:14:54 gleixner Exp $
 *
 * Copyright (C) 1999-2003 David Woodhouse <dwmw2@infradead.org> et al.
 *
 * Released under GPL
 */

#ifndef __MTD_MTD_H__
#define __MTD_MTD_H__

#include <driver.h>
#include <errno.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/mtd/mtd-abi.h>
#include <linux/math64.h>

#define MTD_CHAR_MAJOR 90
#define MTD_BLOCK_MAJOR 31
#define MAX_MTD_DEVICES 32

#define MTD_FAIL_ADDR_UNKNOWN -1LL

/* If the erase fails, fail_addr might indicate exactly which block failed.  If
   fail_addr = 0xffffffff, the failure was not at the device level or was not
   specific to any particular block. */
struct erase_info {
	struct mtd_info *mtd;
	u_int64_t addr;
	u_int64_t len;
	u_int64_t fail_addr;
	u_long time;
	u_long retries;
	u_int dev;
	u_int cell;
	u_long priv;
	struct erase_info *next;
};

struct mtd_erase_region_info {
	u_int64_t offset;			/* At which this region starts, from the beginning of the MTD */
	u_int32_t erasesize;		/* For this region */
	u_int32_t numblocks;		/* Number of blocks of erasesize in this region */
	unsigned long *lockmap;		/* If keeping bitmap of locks */
};

/**
 * struct mtd_oob_ops - oob operation operands
 * @mode:	operation mode
 *
 * @len:	number of data bytes to write/read
 *
 * @retlen:	number of data bytes written/read
 *
 * @ooblen:	number of oob bytes to write/read
 * @oobretlen:	number of oob bytes written/read
 * @ooboffs:	offset of oob data in the oob area (only relevant when
 *		mode = MTD_OOB_PLACE)
 * @datbuf:	data buffer - if NULL only oob data are read/written
 * @oobbuf:	oob data buffer
 *
 * Note, it is allowed to read more then one OOB area at one go, but not write.
 * The interface assumes that the OOB write requests program only one page's
 * OOB area.
 */
struct mtd_oob_ops {
	unsigned int	mode;
	size_t		len;
	size_t		retlen;
	size_t		ooblen;
	size_t		oobretlen;
	uint32_t	ooboffs;
	uint8_t		*datbuf;
	uint8_t		*oobbuf;
};

#define MTD_MAX_OOBFREE_ENTRIES_LARGE	32
#define MTD_MAX_ECCPOS_ENTRIES_LARGE	640
/**
 * struct mtd_oob_region - oob region definition
 * @offset: region offset
 * @length: region length
 *
 * This structure describes a region of the OOB area, and is used
 * to retrieve ECC or free bytes sections.
 * Each section is defined by an offset within the OOB area and a
 * length.
 */
struct mtd_oob_region {
	u32 offset;
	u32 length;
};

/*
 * struct mtd_ooblayout_ops - NAND OOB layout operations
 * @ecc: function returning an ECC region in the OOB area.
 *	 Should return -ERANGE if %section exceeds the total number of
 *	 ECC sections.
 * @free: function returning a free region in the OOB area.
 *	  Should return -ERANGE if %section exceeds the total number of
 *	  free sections.
 */
struct mtd_ooblayout_ops {
	int (*ecc)(struct mtd_info *mtd, int section,
		   struct mtd_oob_region *oobecc);
	int (*free)(struct mtd_info *mtd, int section,
		    struct mtd_oob_region *oobfree);
};

struct mtd_info {
	u_char type;
	u_int32_t flags;
	u_int64_t size;	 /* Total size of the MTD */

	/* "Major" erase size for the device. Naïve users may take this
	 * to be the only erase size available, or may use the more detailed
	 * information below if they desire
	 */
	u_int32_t erasesize;
	/* Minimal writable flash unit size. In case of NOR flash it is 1 (even
	 * though individual bits can be cleared), in case of NAND flash it is
	 * one NAND page (or half, or one-fourths of it), in case of ECC-ed NOR
	 * it is of ECC block size, etc. It is illegal to have writesize = 0.
	 * Any driver registering a struct mtd_info must ensure a writesize of
	 * 1 or larger.
	 */
	u_int32_t writesize;

	/*
	 * Size of the write buffer used by the MTD. MTD devices having a write
	 * buffer can write multiple writesize chunks at a time. E.g. while
	 * writing 4 * writesize bytes to a device with 2 * writesize bytes
	 * buffer the MTD driver can (but doesn't have to) do 2 writesize
	 * operations, but not 4. Currently, all NANDs have writebufsize
	 * equivalent to writesize (NAND page size). Some NOR flashes do have
	 * writebufsize greater than writesize.
	 */
	uint32_t writebufsize;

	u_int32_t oobsize;   // Amount of OOB data per block (e.g. 16)
	u_int32_t oobavail;  // Available OOB bytes per block

	/*
	 * read ops return -EUCLEAN if max number of bitflips corrected on any
	 * one region comprising an ecc step equals or exceeds this value.
	 * Settable by driver, else defaults to ecc_strength.  User can override
	 * in sysfs.  N.B. The meaning of the -EUCLEAN return code has changed;
	 * see Documentation/ABI/testing/sysfs-class-mtd for more detail.
	 */
	unsigned int bitflip_threshold;

	// Kernel-only stuff starts here.
	char *name;
	int index;

	/* ecc layout structure pointer - read only ! */
	struct nand_ecclayout *ecclayout;
	/* OOB layout description */
	const struct mtd_ooblayout_ops *ooblayout;

	/* the ecc step size. */
	unsigned int ecc_step_size;

	/* max number of correctible bit errors per ecc step */
	unsigned int ecc_strength;

	/* Data for variable erase regions. If numeraseregions is zero,
	 * it means that the whole device has erasesize as given above.
	 */
	int numeraseregions;
	struct mtd_erase_region_info *eraseregions;

	int (*_erase) (struct mtd_info *mtd, struct erase_info *instr);

	int (*_read) (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf);
	int (*_write) (struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const u_char *buf);

	int (*_read_oob) (struct mtd_info *mtd, loff_t from,
			 struct mtd_oob_ops *ops);
	int (*_write_oob) (struct mtd_info *mtd, loff_t to,
			 struct mtd_oob_ops *ops);

	/*
	 * Methods to access the protection register area, present in some
	 * flash devices. The user data is one time programmable but the
	 * factory data is read only.
	 */
	int (*_get_fact_prot_info) (struct mtd_info *mtd, struct otp_info *buf, size_t len);
	int (*_read_fact_prot_reg) (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf);
	int (*_get_user_prot_info) (struct mtd_info *mtd, struct otp_info *buf, size_t len);
	int (*_read_user_prot_reg) (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf);
	int (*_write_user_prot_reg) (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf);
	int (*_lock_user_prot_reg) (struct mtd_info *mtd, loff_t from, size_t len);

	/* Sync */
	void (*_sync) (struct mtd_info *mtd);

	/* Chip-supported device locking */
	int (*_lock) (struct mtd_info *mtd, loff_t ofs, size_t len);
	int (*_unlock) (struct mtd_info *mtd, loff_t ofs, size_t len);

	/* Bad block management functions */
	int (*_block_isbad) (struct mtd_info *mtd, loff_t ofs);
	int (*_block_markbad) (struct mtd_info *mtd, loff_t ofs);
	int (*_block_markgood) (struct mtd_info *mtd, loff_t ofs);

	int (*of_fixup)(struct mtd_info *mtd, struct device_node *root);

	/* ECC status information */
	struct mtd_ecc_stats ecc_stats;
	/* Subpage shift (NAND) */
	int subpage_sft;

	void *priv;

	struct module *owner;
	int usecount;

	/* If the driver is something smart, like UBI, it may need to maintain
	 * its own reference counting. The below functions are only for driver.
	 * The driver may register its callbacks. These callbacks are not
	 * supposed to be called by MTD users */
	int (*_get_device) (struct mtd_info *mtd);
	void (*_put_device) (struct mtd_info *mtd);

	struct device dev;
	struct cdev cdev;

	struct cdev *cdev_bb;

	struct param_d param_size;
	char *size_str;

	/* If true erasing bad blocks is allowed, this is set via a device parameter */
	bool allow_erasebad;
	int p_allow_erasebad;

	/*
	 * Parent device from the MTD partition point of view.
	 *
	 * MTD masters do not have any parent, MTD partitions do. The parent
	 * MTD device can itself be a partition.
	 */
	struct mtd_info *parent;
	loff_t master_offset;

	struct list_head partitions;
	struct list_head partitions_entry;
	char *partition_string;

	unsigned int of_binding;
};

int mtd_ooblayout_ecc(struct mtd_info *mtd, int section,
		      struct mtd_oob_region *oobecc);
int mtd_ooblayout_find_eccregion(struct mtd_info *mtd, int eccbyte,
				 int *section,
				 struct mtd_oob_region *oobregion);
int mtd_ooblayout_get_eccbytes(struct mtd_info *mtd, u8 *eccbuf,
			       const u8 *oobbuf, int start, int nbytes);
int mtd_ooblayout_set_eccbytes(struct mtd_info *mtd, const u8 *eccbuf,
			       u8 *oobbuf, int start, int nbytes);
int mtd_ooblayout_free(struct mtd_info *mtd, int section,
		       struct mtd_oob_region *oobfree);
int mtd_ooblayout_get_databytes(struct mtd_info *mtd, u8 *databuf,
				const u8 *oobbuf, int start, int nbytes);
int mtd_ooblayout_set_databytes(struct mtd_info *mtd, const u8 *databuf,
				u8 *oobbuf, int start, int nbytes);
int mtd_ooblayout_count_freebytes(struct mtd_info *mtd);
int mtd_ooblayout_count_eccbytes(struct mtd_info *mtd);

static inline void mtd_set_ooblayout(struct mtd_info *mtd,
				     const struct mtd_ooblayout_ops *ooblayout)
{
	mtd->ooblayout = ooblayout;
}

int mtd_erase(struct mtd_info *mtd, struct erase_info *instr);
int mtd_read(struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen,
	     u_char *buf);
int mtd_write(struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen,
	      const u_char *buf);

int mtd_read_oob(struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops);

static inline void mtd_set_of_node(struct mtd_info *mtd,
				   struct device_node *np)
{
	mtd->dev.of_node = np;
}

static inline struct device_node *mtd_get_of_node(struct mtd_info *mtd)
{
	if (!IS_ENABLED(CONFIG_OFDEVICE))
		return NULL;
	if (mtd->dev.of_node)
		return mtd->dev.of_node;
	if (mtd->dev.parent)
		return mtd->dev.parent->of_node;
	return NULL;
}

static inline u32 mtd_oobavail(struct mtd_info *mtd, struct mtd_oob_ops *ops)
{
	return ops->mode == MTD_OPS_AUTO_OOB ? mtd->oobavail : mtd->oobsize;
}

int mtd_write_oob(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops);

static inline int mtd_can_have_bb(const struct mtd_info *mtd)
{
	return !!mtd->_block_isbad;
}

static inline uint32_t mtd_div_by_eb(uint64_t sz, struct mtd_info *mtd)
{
	do_div(sz, mtd->erasesize);
	return sz;
}

static inline uint32_t mtd_mod_by_eb(uint64_t sz, struct mtd_info *mtd)
{
	return do_div(sz, mtd->erasesize);
}

static inline uint32_t mtd_div_by_wb(uint64_t sz, struct mtd_info *mtd)
{
	do_div(sz, mtd->writesize);
	return sz;
}

	/* Kernel-side ioctl definitions */

extern int add_mtd_device(struct mtd_info *mtd, const char *devname, int device_id);
extern int del_mtd_device (struct mtd_info *mtd);

extern struct mtd_info *get_mtd_device(struct mtd_info *mtd, int num);
extern struct mtd_info *get_mtd_device_nm(const char *name);

extern void put_mtd_device(struct mtd_info *mtd);

const char *mtd_type_str(struct mtd_info *mtd);

struct mtd_notifier {
	void (*add)(struct mtd_info *mtd);
	void (*remove)(struct mtd_info *mtd);
	struct list_head list;
};

struct mtd_info *mtd_add_partition(struct mtd_info *mtd, off_t offset,
		uint64_t size, unsigned long flags, const char *name);
int mtd_del_partition(struct mtd_info *mtd);

extern void register_mtd_user (struct mtd_notifier *new);
extern int unregister_mtd_user (struct mtd_notifier *old);

int mtd_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len);
int mtd_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len);
int mtd_block_isbad(struct mtd_info *mtd, loff_t ofs);
int mtd_block_markbad(struct mtd_info *mtd, loff_t ofs);
int mtd_block_markgood(struct mtd_info *mtd, loff_t ofs);

int mtd_buf_all_ff(const void *buf, unsigned int len);
int mtd_buf_check_pattern(const void *buf, uint8_t patt, int size);

static inline int mtd_is_bitflip(int err) {
	return err == -EUCLEAN;
}

static inline int mtd_is_eccerr(int err) {
	return err == -EBADMSG;
}

static inline int mtd_is_bitflip_or_eccerr(int err) {
	return mtd_is_bitflip(err) || mtd_is_eccerr(err);
}

void mtd_set_ecclayout(struct mtd_info *mtd, struct nand_ecclayout *ecclayout);

void mtd_print_oob_info(struct mtd_info *mtd);

#endif /* __MTD_MTD_H__ */
