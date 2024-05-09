/*
 * src/plugins/mfs3.c
 * https://gitlab.com/bztsrc/easyboot
 *
 * Copyright (C) 2023 bzt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @brief Implements the Minix3 filesystem
 * https://gitlab.com/bztsrc/minix3fs
 */

#include "../loader.h"

EASY_PLUGIN(PLG_T_FS) {
    { 0x418, 2, PLG_M_CONST, { 0x5A, 0x4D, 0, 0 } }
};

#define MFS_ROOT_INO    1       /* Root inode */

#define MFS_DIRSIZ     60
#define NR_DZONES       7       /* # direct zone numbers in a V2 inode */
#define NR_TZONES      10       /* total # zone numbers in a V2 inode */

/* i_mode */
#define MFS_IFDIR              0x4000
#define MFS_IFREG              0x8000
#define MFS_IFLNK              0xA000
#define MFS_IFMSK              0xF000

typedef struct {
  uint32_t s_ninodes;           /* # usable inodes on the minor device */
  uint16_t s_nzones;            /* total device size, including bit maps etc */
  uint16_t s_imap_blocks;       /* # of blocks used by inode bit map */
  uint16_t s_zmap_blocks;       /* # of blocks used by zone bit map */
  uint16_t s_firstdatazone_old; /* number of first data zone (small) */
  uint16_t s_log_zone_size;     /* log2 of blocks/zone */
  uint16_t s_flags;             /* FS state flags */
  int32_t s_max_size;           /* maximum file size on this device */
  uint32_t s_zones;             /* number of zones (replaces s_nzones in V2) */
  int16_t s_magic;              /* magic number to recognize super-blocks */
  /* The following items are valid on disk only for V3 and above */
  int16_t s_pad2;               /* try to avoid compiler-dependent padding */
  uint16_t s_block_size;        /* block size in bytes. */
  int8_t s_disk_version;        /* filesystem format sub-version */
} __attribute__((packed)) superblock_t;

typedef struct {                /* V2/V3 disk inode */
  uint16_t i_mode;              /* file type, protection, etc. */
  uint16_t i_nlinks;            /* how many links to this file. */
  int16_t i_uid;                /* user id of the file's owner. */
  uint16_t i_gid;               /* group number */
  uint32_t i_size;              /* current file size in bytes */
  uint32_t i_atime;             /* when was file data last accessed */
  uint32_t i_mtime;             /* when was file data last changed */
  uint32_t i_ctime;             /* when was inode data last changed */
  uint32_t i_zone[NR_TZONES];   /* zone nums for direct, ind, and dbl ind */
} __attribute__((packed)) inode_t;

typedef struct {
  uint32_t d_ino;
  char d_name[MFS_DIRSIZ];
} __attribute__((packed)) direct_t;

uint64_t ind[2], inode_table;
uint32_t block_size, sec_per_blk, blk_per_block, blk_per_block2, mfs_inode, *blklst;
uint8_t data[32768], dir[32768];
inode_t *ino;

/**
 * Load a block
 */
static void loadblk(uint64_t blk, uint8_t *dst)
{
    uint32_t i;
    blk *= sec_per_blk;
    for(i = 0; i < sec_per_blk; i++, blk++, dst += 512)
        loadsec(blk, dst);
}

/**
 * Load an inode
 */
static void loadinode(uint32_t inode)
{
    uint32_t block_offs = (inode - 1) * sizeof(inode_t) / block_size;
    uint32_t inode_offs = (inode - 1) * sizeof(inode_t) % block_size;
    loadblk(inode_table + block_offs, root_buf);
    /* copy inode from the table into the beginning of our buffer */
    memcpy(root_buf, root_buf + inode_offs, sizeof(inode_t));
    memset(ind, 0, sizeof(ind));
    mfs_inode = inode;
}

/**
 * Read from opened file
 */
static uint64_t mfs_read(uint64_t offs, uint64_t size, void *buf)
{
    uint64_t blk, rem;
    uint32_t i, j, k, os = 0, rs = block_size;

    if(!mfs_inode || offs >= ino->i_size || !size || !buf) return 0;
    if(offs + size > ino->i_size) size = ino->i_size - offs;
    rem = size;

    pb_init(size);
    os = offs % block_size;
    offs /= block_size;
    if(os) rs = block_size - os;
    while(rem) {
        /* convert file offset block number to file system block number */
        /* we keep indirect records cached in blklst, and the block numbers where they were loaded from in ind[X]
         *   ind[0] -> blklst[0 .. blk_per_block-1]
         *   ind[1] -> blklst[blk_per_block .. 2*blk_per_block-1] */
        blk = 0;
        if(offs < NR_DZONES) { /* direct */ blk = ino->i_zone[offs]; } else
        if(offs >= NR_DZONES && offs < NR_DZONES + blk_per_block) {
            /* indirect */
            if(ind[0] != ino->i_zone[NR_DZONES]) { ind[0] = ino->i_zone[NR_DZONES]; loadblk(ind[0], (uint8_t*)blklst); }
            blk = blklst[offs - NR_DZONES];
        } else
        if(offs >= NR_DZONES + blk_per_block && offs < NR_DZONES + blk_per_block + blk_per_block2) {
            /* double indirect */
            i = offs - NR_DZONES - blk_per_block;
            k = NR_DZONES + 1 + i / blk_per_block2;
            if(k < NR_TZONES) {
                if(ind[0] != ino->i_zone[k]) { ind[0] = ino->i_zone[k]; loadblk(ind[0], (uint8_t*)blklst); }
                j = blklst[i / blk_per_block];
                if(ind[1] != j) { ind[1] = j; loadblk(ind[1], (uint8_t*)blklst + block_size); }
                blk = blklst[blk_per_block + (i % blk_per_block)];
            }
        }
        if(!blk) break;
        loadblk(blk, data);
        if(rs > rem) rs = rem;
        memcpy(buf, data + os, rs); os = 0;
        buf += rs; rem -= rs; rs = block_size;
        pb_draw(size - rem);
        offs++;
    }
    pb_fini();
    return (size - rem);
}

/**
 * Look up directories and return inode and set file_size
 */
static int mfs_open(char *fn)
{
    direct_t *de;
    uint64_t offs;
    uint32_t i, rem, redir = 0;
    char *s = fn, *e;

    if(!fn || !*fn) return 0;

    file_size = 0;
again:
    loadinode(MFS_ROOT_INO);
    for(e = s; *e && *e != '/'; e++);
    offs = 0;
    while(offs < ino->i_size) {
        /* read in the next block in the directory */
        if(!mfs_read(offs, block_size, dir)) break;
        rem = ino->i_size - offs; if(rem > block_size) rem = block_size;
        offs += block_size;
        /* check filenames in directory entries */
        for(i = 0, de = (direct_t*)dir; i < rem; i += sizeof(direct_t), de++)
            if(de->d_ino && e - s < MFS_DIRSIZ && !memcmp(s, (uint8_t*)de->d_name, e - s) && !de->d_name[e - s]) {
                loadinode(de->d_ino);
                /* symlink */
                if((ino->i_mode & MFS_IFMSK) == MFS_IFLNK) {
                    i = ino->i_size;
                    if(i > PATH_MAX - 1) i = PATH_MAX - 1;
                    if(redir >= 8 || !mfs_read(0, i, dir)) goto err;
                    dir[i] = 0; redir++;
                    if(dir[0] == '/') memcpy(fn, dir + 1, i);
                    else {
                        for(e = (char*)dir; *e && s < &fn[PATH_MAX - 1]; e++) {
                            if(e[0] == '.' && e[1] == '/') e++; else
                            if(e[0] == '.' && e[1] == '.' && e[2] == '/') {
                                e += 2; if(s > fn) for(s--; s > fn && s[-1] != '/'; s--);
                            } else *s++ = *e;
                        }
                        *s = 0;
                    }
                    s = fn; goto again;
                }
                /* regular file and end of path */
                if(!*e) { if((ino->i_mode & MFS_IFMSK) == MFS_IFREG) { file_size = ino->i_size; return 1; } goto err; }
                /* directory and not end of path */
                if((ino->i_mode & MFS_IFMSK) == MFS_IFDIR) {
                    offs = 0; s = e + 1; for(e = s; *e && *e != '/'; e++);
                    break;
                } else
                /* anything else (device, fifo etc.) or file in the middle or directory at the end */
                    goto err;
            }
    }
err:mfs_inode = 0;
    return 0;
}

/**
 * Close the opened file
 */
static void mfs_close(void)
{
    mfs_inode = 0;
}

/**
 * Plugin entry point, check superblock and call sethooks
 */
PLG_API void _start(void)
{
    superblock_t *sb = (superblock_t*)(root_buf + 1024);

    /* save values from the superblock */
    block_size = sb->s_block_size;
    sec_per_blk = block_size >> 9;
    blk_per_block = block_size / sizeof(uint32_t); blk_per_block2 = blk_per_block * blk_per_block;
    inode_table = 2 + sb->s_imap_blocks + sb->s_zmap_blocks;
    mfs_inode = 0; ino = (inode_t*)root_buf; blklst = (uint32_t*)(root_buf + sizeof(inode_t));

    sethooks(mfs_open, mfs_read, mfs_close);
}
