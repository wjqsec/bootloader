/*
 * src/plugins/fsZ.c
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
 * @brief Implements the FS/Z filesystem
 * https://gitlab.com/bztsrc/bootboot/-/raw/binaries/specs/fsz.pdf
 */

#include "../loader.h"

EASY_PLUGIN(PLG_T_FS) {
    { 0x200, 2, PLG_M_CONST, { 'F', 'S', '/', 'Z' } }
};

typedef struct {
    uint8_t     loader[512];
    uint8_t     magic[4];
    uint8_t     version_major;
    uint8_t     version_minor;
    uint8_t     logsec;
    uint8_t     flags;
    uint32_t    enchash;
    uint16_t    maxmounts;
    uint16_t    currmounts;
    uint64_t    numsec;
    uint64_t    numsec_hi;
    uint64_t    freesec;
    uint64_t    freesec_hi;
    uint64_t    rootdirfid;
    uint64_t    rootdirfid_hi;
    uint64_t    freesecfid;
    uint64_t    freesecfid_hi;
    uint64_t    badsecfid;
    uint64_t    badsecfid_hi;
    uint64_t    indexfid;
    uint64_t    indexfid_hi;
    uint64_t    metafid;
    uint64_t    metafid_hi;
    uint64_t    journalfid;
    uint64_t    journalfid_hi;
    uint64_t    journalhead;
    uint64_t    journaltail;
    uint64_t    journalmax;
    uint8_t     encrypt[32];
    uint64_t    createdate;
    uint64_t    lastmountdate;
    uint64_t    lastumountdate;
    uint64_t    lastcheckdate;
    uint8_t     uuid[16];
    uint8_t     reserved[256];
    uint8_t     magic2[4];
    uint32_t    checksum;
} __attribute__((packed)) fsz_superblock_t;

typedef struct {
    uint8_t       magic[4];
    uint32_t      checksum;
    uint8_t       filetype[4];
    uint8_t       mimetype[60];
    uint64_t      createdate;
    uint64_t      changedate;
    uint64_t      accessdate;
    uint64_t      numblocks;
    uint64_t      numlinks;
    uint64_t      metasec;
    uint64_t      metasec_hi;
    uint8_t       versions[5*64];
    uint64_t         sec;
    uint64_t         sec_hi;
    uint64_t         size;
    uint64_t         size_hi;
    uint64_t         modifydate;
    uint64_t         flags;
    guid_t        owner;
    guid_t        acl[32];
} __attribute__((packed)) fsz_inode_t;

#define FSZ_IN_FLAG_LEVEL(x) ((x)&15)
#define FSZ_IN_FLAG_SECLIST  (1<<4)

typedef struct {
    uint64_t      sec;
    uint64_t      sec_hi;
    uint64_t      numsec;
    uint32_t      numsec_hi;
    uint32_t      chksum;
} __attribute__((packed)) fsz_seclist_t;

typedef struct {
    uint64_t      sec;
    uint32_t      sec_hi;
    uint32_t      chksum;
} __attribute__((packed)) fsz_secdir_t;

typedef struct {
    uint64_t    fid;
    uint64_t    fid_hi;
    uint8_t     name[112];
} __attribute__((packed)) fsz_dirent_t;

uint64_t fsz_inode, root_inode, ind[2];
uint32_t block_size, sec_per_blk, sd_per_block;
uint8_t data[32768], dir[32768], *blklst;
fsz_inode_t *ino;

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
    loadblk(inode, root_buf);
    memset(ind, 0, sizeof(ind));
    fsz_inode = inode;
}

/**
 * Read from opened file
 */
static uint64_t fsz_read(uint64_t offs, uint64_t size, void *buf)
{
    uint64_t blk, pos, rem;
    uint32_t i, j, os = 0, rs = block_size;
    fsz_seclist_t *sl;

    if(!fsz_inode || offs >= ino->size || !size || !buf) return 0;
    if(offs + size > ino->size) size = ino->size - offs;
    rem = size;

    pb_init(size);
    os = offs % block_size;
    offs /= block_size;
    if(os) rs = block_size - os;
    while(rem) {
        /* convert file offset block number to file system block number */
        /* we keep indirect records cached in blklst, and the block numbers where they were loaded from in ind[X] */
        blk = 0;
        if(ino->flags & FSZ_IN_FLAG_SECLIST) {
            /* sector list */
            if(FSZ_IN_FLAG_LEVEL(ino->flags) == 0) {
                if(fsz_inode == ino->sec) {
                    /* inlined sector list (up to 96 extents with 4096 block) */
                    sl = (fsz_seclist_t*)(root_buf + sizeof(fsz_inode_t));
                    j = (block_size - sizeof(fsz_inode_t)) / sizeof(fsz_seclist_t);
                } else {
                    /* normal sector list (up to 128 extents with 4096 block) */
                    if(ind[0] != ino->sec) { ind[0] = ino->sec; loadblk(ind[0], blklst); }
                    sl = (fsz_seclist_t*)blklst;
                    j = block_size / sizeof(fsz_seclist_t);
                }
                /* find the extent */
                for(pos = 0, i = 0; !blk && i < j; pos += sl->numsec, i++, sl++)
                    if(offs >= pos && offs < pos + sl->numsec)
                        blk = sl->sec + (offs - pos);
            } else {
                /* indirect sector list, not supported here */
            }
            if(!blk) break;
        } else {
            /* sector directory */
            if(FSZ_IN_FLAG_LEVEL(ino->flags) > 2) break;
            switch(FSZ_IN_FLAG_LEVEL(ino->flags)) {
                case 0: /* direct */
                    if(fsz_inode == ino->sec) { /* inlined data */ memcpy(data, root_buf + sizeof(fsz_inode_t), ino->size); }
                    else { /* direct data (up to block size) */ blk = ino->sec; }
                break;
                case 1: /* indirect (up to 1M with 4096 block) */
                    if(fsz_inode == ino->sec) {
                        /* inlined sector directory */
                        blk = ((fsz_secdir_t*)(root_buf + sizeof(fsz_inode_t)))[offs].sec;
                    } else {
                        /* indirect sector directory */
                        if(ind[0] != ino->sec) { ind[0] = ino->sec; loadblk(ind[0], blklst); }
                        blk = ((fsz_secdir_t*)blklst)[offs].sec;
                    }
                break;
                case 2: /* double indirect (up to 256M with 4096 block)  */
                    if(ind[0] != ino->sec) { ind[0] = ino->sec; loadblk(ind[0], blklst); }
                    j = ((fsz_secdir_t*)blklst)[offs / sd_per_block].sec;
                    if(ind[1] != j) { ind[1] = j; loadblk(ind[1], blklst + block_size); }
                    blk = ((fsz_secdir_t*)(blklst + block_size))[offs % sd_per_block].sec;
                break;
            }
        }
        if(blk) loadblk(blk, data);
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
static int fsz_open(char *fn)
{
    fsz_dirent_t *de;
    uint64_t offs;
    uint32_t i, rem, redir = 0;
    char *s = fn, *e;

    if(!fn || !*fn) return 0;

    file_size = 0;
again:
    loadinode(root_inode);
    for(e = s; *e && *e != '/'; e++);
    offs = 0;
    while(offs < ino->size) {
        /* read in the next block in the directory */
        if(!fsz_read(offs, block_size, dir)) break;
        rem = ino->size - offs; if(rem > block_size) rem = block_size;
        /* skip over directory entry header */
        de = (fsz_dirent_t*)dir; if(!offs) { rem -= sizeof(fsz_dirent_t); de++; }
        offs += block_size;
        /* check filenames in directory entries */
        for(i = 0; i < rem; i += sizeof(fsz_dirent_t), de++)
            if(de->fid && e - s < 112 && !memcmp(s, (uint8_t*)de->name, e - s) && !de->name[e - s]) {
                loadinode(de->fid);
                /* symlink */
                if(!memcmp(&ino->filetype, "lnk:", 4)) {
                    i = ino->size;
                    if(i > PATH_MAX - 1) i = PATH_MAX - 1;
                    if(redir >= 8 || !fsz_read(0, i, dir)) goto err;
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
                if(!*e) { if(ino->filetype[3] != ':') { file_size = ino->size; return 1; } goto err; }
                /* directory and not end of path */
                if(!memcmp(&ino->filetype, "dir:", 4)) {
                    offs = 0; s = e + 1; for(e = s; *e && *e != '/'; e++);
                    break;
                } else
                /* anything else (device, fifo etc.) or file in the middle or directory at the end */
                    goto err;
            }
    }
err:fsz_inode = 0;
    return 0;
}

/**
 * Close the opened file
 */
static void fsz_close(void)
{
    fsz_inode = 0;
}

PLG_API void _start(void)
{
    fsz_superblock_t *sb = (fsz_superblock_t*)root_buf;

    /* extra features, encryption and block size above 32k not supported here */
    if(sb->flags || sb->enchash || sb->logsec > 4) return;

    /* save values from the superblock */
    block_size = 1 << (sb->logsec + 11);
    sec_per_blk = block_size >> 9; sd_per_block = block_size / sizeof(fsz_secdir_t);
    root_inode = sb->rootdirfid;
    fsz_inode = 0; ino = (fsz_inode_t*)root_buf; blklst = root_buf + block_size;
    sethooks(fsz_open, fsz_read, fsz_close);
}
