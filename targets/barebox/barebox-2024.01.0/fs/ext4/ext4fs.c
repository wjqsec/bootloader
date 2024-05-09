/*
 * (C) Copyright 2011 - 2012 Samsung Electronics
 * EXT4 filesystem implementation in Uboot by
 * Uma Shankar <uma.shankar@samsung.com>
 * Manjunatha C Achar <a.manjunatha@samsung.com>
 *
 * ext4ls and ext4load : Based on ext2 ls and load support in Uboot.
 *		       Ext4 read optimization taken from Open-Moko
 *		       Qi bootloader
 *
 * (C) Copyright 2004
 * esd gmbh <www.esd-electronics.com>
 * Reinhard Arlt <reinhard.arlt@esd-electronics.com>
 *
 * based on code from grub2 fs/ext2.c and fs/fshelp.c by
 * GRUB  --  GRand Unified Bootloader
 * Copyright (C) 2003, 2004  Free Software Foundation, Inc.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <malloc.h>
#include <linux/stat.h>
#include <linux/time.h>
#include <asm/byteorder.h>
#include "ext4_common.h"

void ext4fs_free_node(struct ext2fs_node *node, struct ext2fs_node *currroot)
{
	if ((node != &node->data->diropen) && (node != currroot))
		free(node);
}

/*
 * Taken from openmoko-kernel mailing list: By Andy green
 * Optimized read file API : collects and defers contiguous sector
 * reads into one potentially more efficient larger sequential read action
 */
loff_t ext4fs_read_file(struct ext2fs_node *node, loff_t pos,
		unsigned int len, char *buf)
{
	loff_t i;
	blkcnt_t blockcnt;
	int log2blocksize = LOG2_EXT2_BLOCK_SIZE(node->data);
	const int blockshift = log2blocksize + DISK_SECTOR_BITS;
	const int blocksize = 1 << blockshift;
	loff_t filesize = ext4_isize(node);
	ssize_t ret;
	struct ext_filesystem *fs = node->data->fs;

	/* Adjust len so it we can't read past the end of the file. */
	if (len + pos > filesize)
		len = filesize - pos;

	if (filesize <= pos)
		return -EINVAL;

	blockcnt = ((len + pos) + blocksize - 1) >> blockshift;

	for (i = pos >> blockshift; i < blockcnt; i++) {
		sector_t blknr;
		loff_t blockoff = pos - (blocksize * i);
		loff_t blockend = blocksize;
		loff_t skipfirst = 0;

		ret = read_allocated_block(node, i);
		if (ret < 0)
			return ret;

		blknr = ret << log2blocksize;

		/* Last block.  */
		if (i == blockcnt - 1) {
			blockend = (len + pos) - (blocksize * i);

			/* The last portion is exactly blocksize. */
			if (!blockend)
				blockend = blocksize;
		}

		/* First block. */
		if (i == pos >> blockshift) {
			skipfirst = blockoff;
			blockend -= skipfirst;
		}

		if (blknr) {
			ret = ext4fs_devread(fs, blknr, skipfirst, blockend, buf);
			if (ret)
				return ret;
		} else {
			memset(buf, 0, blockend);
		}

		buf += blocksize - skipfirst;
	}

	return len;
}
