/*
 * src/plugins/axfs.c
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
 * @brief Implements the AleXandria File System
 * https://gitlab.com/bztsrc/alexandriafs
 *
 * Supported features
 * - inlined data
 * - block sizes 515 bytes to 64k
 * - inode sizes 256 bytes to 64k
 * - single, double, triple indirect block runs
 */

#include "../loader.h"

EASY_PLUGIN(PLG_T_FS) {
    { 0x04, 4, PLG_M_CONST, { 'A', 'X', 'F', 'S' } },
    { 0xFC, 4, PLG_M_CONST, { 'A', 'X', 'F', 'S' } }
};

typedef struct {
    uint8_t         s_loader[4];
    uint8_t         s_magic[4];
    uint8_t         s_major;
    uint8_t         s_minor;
    uint8_t         s_blksize;
    uint8_t         s_inoshift;
    uint16_t        s_flags;
    uint8_t         s_nresblks;
    uint8_t         s_enctype;
    uint8_t         s_encrypt[28];
    uint32_t        s_enchash;
    uint8_t         s_uuid[16];
    uint64_t        s_nblks;
    uint64_t        s_volume_fid;
    uint64_t        s_root_fid;
    uint64_t        s_freeblk_fid;
    uint64_t        s_journal_fid;
    uint64_t        s_attridx_fid;
    uint64_t        s_uquota_fid;
    uint8_t         s_reserved[120];
    uint64_t        s_badblk_fid;
    uint32_t        s_chksum;
    uint8_t         s_magic2[4];
} __attribute__((packed)) axfs_super_t;

typedef struct {
    uint64_t        e_blk;
    uint64_t        e_num;
    uint32_t        e_len;
} __attribute__((packed)) axfs_extent_t;

#define AXFS_IN_MAGIC    "AXIN"
#define AXFS_FT_DIR      0
#define AXFS_FT_LNK      3
#define AXFS_FT_REG      8  /* and above */

typedef struct {
    uint8_t         i_magic[4];
    uint32_t        i_chksum;
    uint64_t        i_nblks;
    uint64_t        i_size;
    uint16_t        i_size_hi;
    uint8_t         i_flags;
    uint8_t         i_type;
    uint32_t        i_mime;
    uint64_t        i_nlinks;
    uint64_t        i_btime;
    uint64_t        i_ctime;
    uint64_t        i_mtime;
    uint8_t         i_attr[16];
    uint8_t         i_owner[16];
    uint8_t         i_acl[160];
    uint8_t         i_d[1];
} __attribute__((packed)) axfs_inode_t;

#define AXFS_DR_MAGIC  "AXDR"
typedef struct {
    uint8_t         d_magic[4];
    uint32_t        d_chksum;
    uint32_t        d_size;
    uint32_t        d_nument;
    uint64_t        d_self_fid;
    uint8_t         d_gui_attr[8];
} __attribute__((packed)) axfs_dirhdr_t;

uint64_t axfs_inode, root_fid, ind;
uint32_t block_size, sec_per_blk, inode_size, inode_sh;
uint8_t dir[65536];
axfs_inode_t *ino;

/**
 * Decompress a packed block run into an extent
 */
static uint8_t *unpack(uint8_t *ptr, axfs_extent_t *ext)
{
    register uint8_t bl, nl, ll;

    memset(ext, 0, sizeof(axfs_extent_t));
    if(*ptr) {
        bl = (*ptr) & 7;
        nl = (*ptr >> 3) & 7;
        ll = (*ptr >> 6);
        ptr++;
        if(bl) { bl = 1 << (bl - 1); memcpy(&ext->e_blk, ptr, bl); ptr += bl; }
        if(nl) { nl = 1 << (nl - 1); memcpy(&ext->e_num, ptr, nl); ptr += nl; } else ext->e_num++;
        if(ll) { ll = 1 << (ll - 1); memcpy(&ext->e_len, ptr, ll); ptr += ll; }
    }
    return ptr;
}

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
static void loadinode(uint64_t inode)
{
    uint32_t block_offs = inode >> inode_sh;
    uint32_t inode_offs = (inode & ((1 << inode_sh) - 1)) * inode_size;
    loadblk(block_offs, root_buf);
    /* copy inode from the block into the beginning of our buffer */
    memcpy(root_buf, root_buf + inode_offs, inode_size);
    axfs_inode = !memcmp(root_buf, AXFS_IN_MAGIC, 4) ? inode : 0;
}

/**
 * Read from opened file
 */
static uint64_t axfs_read(uint64_t offs, uint64_t size, void *buf)
{
    uint64_t rem, o = 0, e;
    uint32_t blk, lvl, os = 0, rs = block_size;
    uint8_t *d;
    axfs_extent_t ext;

    if(!axfs_inode || offs >= ino->i_size || !size || !buf) return 0;
    if(offs + size > ino->i_size) size = ino->i_size - offs;
    rem = size;

    pb_init(size);
    os = offs % block_size;
    offs /= block_size;
    if(os) rs = block_size - os;
    while(rem) {
        /* convert file offset block number to file system block number */
        blk = lvl = 0; d = inode_size == 256 ? root_buf + 128 /* ino_small->i_d */: ino->i_d;
        if(rs > rem) rs = rem;
        if(ino->i_flags & 1) {
            /* inlined data */
            memcpy(buf, d + os, rs); blk = 1;
        } else {
            while(*d) {
                d = unpack(d, &ext);
                e = o + ext.e_num * block_size;
                if(o == e) break;
                if(offs >= o && offs < e) {
                    if(ext.e_len) {
                        /* indirect */
                        if(lvl++ > 3) break;
                        d = root_buf + inode_size;
                        if(lvl == 1) {
                            /* keep the first level indirect block cached */
                            if(ext.e_blk != ind) { loadblk(ext.e_blk, d); ind = ext.e_blk; }
                        } else {
                            /* double and triple indirect never used, these are extent runs, but be safe than sorry */
                            loadblk(ext.e_blk, d);
                        }
                    } else {
                        /* direct */
                        if(ext.e_blk) {
                            loadblk(ext.e_blk + o - offs, root_buf + inode_size);
                            memcpy(buf, root_buf + inode_size + os, rs);
                        } else {
                            /* sparse file */
                            memset(buf, 0, rs);
                        }
                        blk = 1;
                        break;
                    }
                } else o = e;
            }
        }
        if(!blk) break;
        os = 0; buf += rs; rem -= rs; rs = block_size;
        pb_draw(size - rem);
        offs++;
    }
    pb_fini();
    return (size - rem);
}

/**
 * Look up directories and return inode and set file_size
 */
static int axfs_open(char *fn)
{
    axfs_extent_t ext;
    uint64_t offs;
    uint32_t i, rem, redir = 0;
    uint8_t *d, *n;
    char *s = fn, *e;

    if(!fn || !*fn) return 0;

    file_size = 0;
again:
    loadinode(root_fid);
    for(e = s; *e && *e != '/'; e++);
    offs = 0;
    while(offs < ino->i_size) {
        /* read in the next block in the directory */
        if(!axfs_read(offs, block_size, dir)) break;
        d = dir;
        /* skip over directory entry header */
        if(!offs) {
            if(!memcmp(dir, AXFS_DR_MAGIC, 4)) break;
            /* chop off the indextable's size */
            ino->i_size = ((axfs_dirhdr_t*)dir)->d_size;
            /* move beyond header */
            offs += sizeof(axfs_dirhdr_t);
            d += sizeof(axfs_dirhdr_t);
        }
        rem = ino->i_size - offs; if(rem > block_size) rem = block_size;
        offs += block_size;
        /* check filenames in directory entries */
        while(d < dir + rem && *d) {
            d = unpack(d, &ext); for(n = d; *d; d++){}; d++;
            if(ext.e_blk && e - s < 247 && !memcmp(s, n, e - s) && !n[e - s]) {
                loadinode(ext.e_blk);
                /* symlink */
                if(ino->i_type == AXFS_FT_LNK) {
                    i = ino->i_size;
                    if(i > PATH_MAX - 1) i = PATH_MAX - 1;
                    if(redir >= 8 || !axfs_read(0, i, dir)) goto err;
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
                if(!*e) { if(ino->i_type < 128 && ino->i_type >= AXFS_FT_REG) { file_size = ino->i_size; return 1; } goto err; }
                /* directory and not end of path */
                if(ino->i_type == AXFS_FT_DIR) {
                    offs = 0; s = e + 1; for(e = s; *e && *e != '/'; e++);
                    break;
                } else
                /* anything else (device, fifo etc.) or file in the middle or directory at the end */
                    goto err;
            }
        }
    }
err:axfs_inode = 0;
    return 0;
}

/**
 * Close the opened file
 */
static void axfs_close(void)
{
    axfs_inode = 0;
}

/**
 * Plugin entry point, check superblock and call sethooks
 */
PLG_API void _start(void)
{
    axfs_super_t *sb = (axfs_super_t*)root_buf;

    /* we support block sizes up to 65536 bytes */
    if(sb->s_blksize > 7 || sb->s_enchash || !sb->s_root_fid) return;

    /* save values from the superblock */
    block_size = 1 << (sb->s_blksize + 9);
    inode_size = block_size >> sb->s_inoshift;
    if(inode_size < 256) return;
    inode_sh = sb->s_inoshift;
    sec_per_blk = block_size >> 9;
    root_fid = sb->s_root_fid;
    axfs_inode = ind = 0; ino = (axfs_inode_t*)root_buf;

    sethooks(axfs_open, axfs_read, axfs_close);
}
