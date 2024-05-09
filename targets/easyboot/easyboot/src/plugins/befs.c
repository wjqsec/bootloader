/*
 * src/plugins/befs.c
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
 * @brief Implements the BeFS filesystem (for Haiku)
 * https://www.haiku-os.org/legacy-docs/practical-file-system-design.pdf
 */

#include "../loader.h"

EASY_PLUGIN(PLG_T_FS) {
    { 512+32, 4, PLG_M_CONST, { '1', 'S', 'F', 'B' } },
    { 512+36, 4, PLG_M_CONST, { 'E', 'G', 'I', 'B' } }
};

#define BEFS_SYMLINK_LEN 144
#define BEFS_NUM_DIRECT_BLOCKS 12
#define BEFS_DBLINDIR_BRUN_LEN 4
#define BEFS_INODE_MAGIC1 0x3bbe0ad9
#define BEFS_BTREE_MAGIC 0x69f6c2e8

/* mode */
#define BEFS_IFDIR        0x4000
#define BEFS_IFREG        0x8000
#define BEFS_IFLNK        0xA000
#define BEFS_IFMSK        0xF000

typedef struct {
    uint32_t    allocation_group;
    uint16_t    start;
    uint16_t    len;
} __attribute__((packed)) block_run_t;

typedef struct {
    char        name[32];
    uint32_t    magic1;
    uint32_t    fs_byte_order;
    uint32_t    block_size;
    uint32_t    block_shift;
    uint64_t    num_blocks;
    uint64_t    used_blocks;
    uint32_t    inode_size;
    uint32_t    magic2;
    uint32_t    blocks_per_ag;
    uint32_t    ag_shift;
    uint32_t    num_ags;
    uint32_t    flags;
    block_run_t log_blocks;
    uint64_t    log_start;
    uint64_t    log_end;
    uint32_t    magic3;
    block_run_t root_dir;
    block_run_t indices;
} __attribute__((packed)) super_block_t;

typedef struct {
    block_run_t direct[BEFS_NUM_DIRECT_BLOCKS];
    uint64_t    max_direct_range;
    block_run_t indirect;
    uint64_t    max_indirect_range;
    block_run_t double_indirect; 
    uint64_t    max_double_indirect_range;
    uint64_t    size;
} __attribute__((packed)) data_stream_t;

typedef struct {
    uint32_t    type;
    uint16_t    name_size;
    uint16_t    data_size;
    char        name[0];
} __attribute__((packed)) small_data_t;

typedef struct {
    uint32_t    magic1;
    block_run_t inode_num;
    uint32_t    uid;
    uint32_t    gid;
    uint32_t    mode;
    uint32_t    flags;
    uint64_t    create_time;
    uint64_t    last_modified_time;
    block_run_t parent;
    block_run_t attributes;
    uint32_t    type;

    uint32_t    inode_size;
    uint32_t    etc; /* not use */

    union {
    data_stream_t datastream;
    char          symlink[BEFS_SYMLINK_LEN];
    } data;

    uint32_t    pad[4]; /* not use */
    small_data_t small_data[0];
} __attribute__((packed)) inode_t;

typedef struct {
    uint32_t    magic;
    uint32_t    node_size;
    uint32_t    max_depth;
    uint32_t    data_type;
    uint64_t    root_node_ptr;
    uint64_t    free_node_ptr;
    uint64_t    max_size;
} __attribute__((packed)) btree_super_t;

typedef struct {
    uint64_t    left;
    uint64_t    right;
    uint64_t    overflow;
    uint16_t    all_key_count;
    uint16_t    all_key_length;
} __attribute__((packed)) btree_node_t;

uint64_t ind[2], root_dir, befs_size;
uint32_t block_size, sec_per_blk, run_per_block, ag_shift, befs_inode;
uint8_t data[65536], dir[65536];
inode_t *ino;
block_run_t *runlst;

/**
 * Get string key at index
 */
static char* get_key(btree_node_t* node, uint16_t index, uint16_t* keylen)
{
    char* keystart;
    uint16_t* keylen_index, prev_key_end;

    if(index > node->all_key_count) { *keylen = 0; return NULL; }
    keystart = (char*)node + sizeof(btree_node_t);
    keylen_index = (uint16_t*)((uint8_t*)node + ((sizeof(btree_node_t) + node->all_key_length + 7) & ~7));
    prev_key_end = index ? keylen_index[index - 1] : 0;
    *keylen = keylen_index[index] - prev_key_end;
    return keystart + prev_key_end;
}

/**
 * Find the uint64_t value for string key
 */
static int find_key(btree_node_t* node, const char* findkey, int len, uint64_t *value)
{
    uint16_t first = 0, mid = 0, last = node->all_key_count - 1;
    int eq = -1;
    uint64_t *valarray;
    uint16_t keylen;
    char *key;

    key = get_key(node, last, &keylen);
    eq = memcmp(key, findkey, len < keylen ? len : keylen);
    if(!eq) eq = keylen - len;
    if(eq < 0) return 0;
    valarray = (uint64_t*)((uint8_t*)node + ((sizeof(btree_node_t) + node->all_key_length + 7) & ~7) +
        node->all_key_count * sizeof(uint16_t));
    while(last >= first) {
        mid = (last + first) / 2;
        key = get_key(node, mid, &keylen);
        eq = memcmp(key, findkey, len < keylen ? len : keylen);
        if(!eq) eq = keylen - len;
        if(!eq) { *value = valarray[mid]; return 1; }
        if(eq > 0) last = mid - 1;
        else first = mid + 1;
    }
    if(eq < 0) *value = valarray[mid +1];
    return 0;
}

/**
 * Convert run to block
 */
static uint64_t run2blk(block_run_t *run)
{
    return (run->allocation_group << ag_shift) + run->start;
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
    loadblk(inode, root_buf);
    memset(ind, 0, sizeof(ind));
    befs_inode = ((inode_t*)root_buf)->magic1 == BEFS_INODE_MAGIC1 ? inode : 0;
    befs_size = ((inode_t*)root_buf)->data.datastream.size;
}

/**
 * Read from opened file
 */
static uint64_t befs_read(uint64_t offs, uint64_t size, void *buf)
{
    uint64_t blk, rem;
    uint32_t i, j, k, l, os = 0, rs = block_size;
    data_stream_t *ds = &((inode_t*)root_buf)->data.datastream;

    if(!befs_inode || offs >= befs_size || !size || !buf) return 0;
    if(offs + size > befs_size) size = befs_size - offs;
    rem = size;

    pb_init(size);
    os = offs % block_size;
    offs /= block_size;
    if(os) rs = block_size - os;
    while(rem) {
        /* convert file offset block number to file system block number */
        /* we keep indirect records cached in runlst */
        blk = 0;
        if(offs * block_size < ds->max_direct_range) {
            /* direct */
            for(i = j = 0; !blk && i < BEFS_NUM_DIRECT_BLOCKS; j += ds->direct[i].len)
                if(offs >= j && offs < j + ds->direct[i].len)
                    blk = run2blk(&ds->direct[i]) + offs - j;
        } else
        if(offs * block_size < ds->max_indirect_range) {
            /* indirect */
            k = run2blk(&ds->indirect);
            for(l = j = 0; !blk && l < ds->indirect.len; l++) {
                if(ind[0] != k + l) { ind[0] = k + l; loadblk(ind[0], (uint8_t*)runlst); }
                for(i = 0; !blk && i < run_per_block; j += runlst[i].len)
                    if(offs >= j && offs < j + runlst[i].len)
                        blk = run2blk(&runlst[i]) + offs - j;
            }
        } else
        if(offs * block_size < ds->max_double_indirect_range) {
            /* double indirect */
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
static int befs_open(char *fn)
{
    btree_super_t ds;
    btree_node_t *node = (btree_node_t *)dir;
    uint64_t offs, inode;
    uint32_t redir = 0;
    char *s = fn, *e;

    if(!fn || !*fn) return 0;

    file_size = 0;
again:
    for(e = s; *e && *e != '/'; e++);
    loadinode(root_dir);
    while(*s) {
        /* read in btree header */
        if(!befs_read(0, sizeof(btree_super_t), &ds) || ds.magic != BEFS_BTREE_MAGIC) goto err;
        /* look up filename in btree */
        offs = ds.root_node_ptr;
        if(!befs_read(offs, block_size, dir)) goto err;
        while(node->overflow != -1ULL) {
            if(!find_key(node, fn, e - s, &offs)) offs = node->overflow;
            if(!befs_read(offs, block_size, dir)) goto err;
        }
        if(!find_key(node, fn, e - s, &inode)) goto err;
        loadinode(inode);
        /* symlink */
        if((ino->mode & BEFS_IFMSK) == BEFS_IFLNK) {
            if(befs_size > PATH_MAX - 1) befs_size = PATH_MAX - 1;
            memcpy(dir, &ino->data.symlink, befs_size); dir[befs_size] = 0;
            redir++; if(redir > 8) goto err;
            if(dir[0] == '/') memcpy(fn, dir + 1, befs_size);
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
        if(!*e) { if((ino->mode & BEFS_IFMSK) == BEFS_IFREG) { file_size = befs_size; return 1; } goto err; }
        /* directory and not end of path */
        if((ino->mode & BEFS_IFMSK) == BEFS_IFDIR) {
            s = e + 1; for(e = s; *e && *e != '/'; e++);
        } else
        /* anything else (device, fifo etc.) or file in the middle or directory at the end */
            break;
    }
err:befs_inode = 0;
    return 0;
}

/**
 * Close the opened file
 */
static void befs_close(void)
{
    befs_inode = 0;
}

/**
 * Plugin entry point, check superblock and call sethooks
 */
PLG_API void _start(void)
{
    super_block_t *sb = (super_block_t*)(root_buf + 512);

    if(sb->block_size != sb->inode_size || (1UL << sb->block_shift) != sb->block_size || sb->block_size > 65536 ||
      sb->num_ags < 1 || sb->ag_shift < 1 || sb->blocks_per_ag < 1 || sb->num_blocks < 10 ||
      sb->num_ags != (sb->num_blocks + (1UL << sb->ag_shift) - 1) / (1UL << sb->ag_shift)) return;

    /* save values from the superblock */
    block_size = sb->block_size;
    sec_per_blk = block_size >> 9;
    run_per_block = block_size / sizeof(block_run_t);
    ag_shift = sb->ag_shift;
    root_dir = run2blk(&sb->root_dir);
    befs_inode = 0; ino = (inode_t*)root_buf; runlst = (block_run_t*)(root_buf + sizeof(inode_t));

    sethooks(befs_open, befs_read, befs_close);
}
