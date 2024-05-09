/*
 * src/plugins/ext234.c
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
 * @brief Implements the ext2/3/4 filesystems
 * https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout
 *
 * Supported ext4 features:
 * - flex block groups
 * - 64 bit addresses
 * - inlined data
 * - extents
 * - hash trees
 * - symlinks (absolute and relative too)
 * Not supported:
 * - meta block groups
 * - compression
 * - encryption
 */

#include "../loader.h"

EASY_PLUGIN(PLG_T_FS) {
    { 1080, 2, PLG_M_CONST, { 0x53, 0xEF, 0, 0 } }
};

#define EXT2_ROOT_INO            2      /* Root inode */

/* s_feature_incompat */
#define EXT2_FEATURE_INCOMPAT_COMPRESSION   0x0001
#define EXT2_FEATURE_INCOMPAT_META_BG       0x0010
#define EXT4_FEATURE_INCOMPAT_64BIT         0x0080
#define EXT4_FEATURE_INCOMPAT_FLEX_BG       0x0200
#define EXT4_FEATURE_INCOMPAT_ENCRYPT       0x10000

/* i_mode */
#define EXT2_IFDIR              0x4000
#define EXT2_IFREG              0x8000
#define EXT2_IFLNK              0xA000
#define EXT2_IFMSK              0xF000

/* i_flags */
#define EXT2_INDEX_FL           0x00001000 /* hash-indexed directory */
#define EXT4_EXTENTS_FL         0x00080000 /* Inode uses extents */
#define EXT4_INLINE_DATA_FL     0x10000000 /* Inode has inline data */

/* ext_eh_t magic */
#define EXT4_EH_MAGIC 0xF30A

typedef struct {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_flags;
    uint32_t bg_exclude_bitmap_lo;
    uint16_t bg_block_bitmap_csum_lo;
    uint16_t bg_inode_bitmap_csum_lo;
    uint16_t bg_itable_unused;
    uint16_t bg_checksum;
    uint32_t bg_block_bitmap_hi;
    uint32_t bg_inode_bitmap_hi;
    uint32_t bg_inode_table_hi;
    uint16_t bg_free_blocks_count_hi;
    uint16_t bg_free_inodes_count_hi;
    uint16_t bg_used_dirs_count_hi;
    uint16_t bg_itable_unused_hi;
    uint32_t bg_exclude_bitmap_hi;
    uint16_t bg_block_bitmap_csum_hi;
    uint16_t bg_inode_bitmap_csum_hi;
    uint32_t bg_reserved;
} __attribute__((packed)) ext_bg_t;

typedef struct {
    uint8_t loader[1024];
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    uint8_t  s_volume_name[16];
    uint8_t  s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_reserved_gdt_blocks;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_jnl_backup_type;
    uint16_t s_desc_size;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_mkfs_time;
    uint32_t s_jnl_blocks[17];
    uint32_t s_blocks_count_hi;
    uint32_t s_r_blocks_count_hi;
    uint32_t s_free_blocks_hi;
    uint16_t s_min_extra_isize;
    uint16_t s_want_extra_isize;
    uint32_t s_flags;
    uint16_t s_raid_stride;
    uint16_t s_mmp_update_interval;
    uint64_t s_mmp_block;
    uint32_t s_raid_stripe_width;
    uint8_t  s_log_groups_per_flex;
} __attribute__((packed)) ext_sb_t;

typedef struct {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_size_hi;
    uint32_t i_faddr;
    uint16_t i_osd2[6];
} __attribute__((packed)) ext_inode_t;

typedef struct {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t type;
} __attribute__((packed)) ext_dirent_t;

typedef struct {
    uint16_t eh_magic;
    uint16_t eh_entries;
    uint16_t eh_max;
    uint16_t eh_depth;
    uint32_t eh_generation;
} __attribute__((packed)) ext_eh_t;

typedef struct {
    uint32_t ei_block;
    uint32_t ei_leaf_lo;
    uint16_t ei_leaf_hi;
    uint16_t ei_unused;
} __attribute__((packed)) ext_ei_t;

typedef struct {
    uint32_t ee_block;
    uint16_t ee_len;
    uint16_t ee_start_hi;
    uint32_t ee_start_lo;
} __attribute__((packed)) ext_ee_t;

uint64_t ind[3], ino_size;
uint32_t blocks_per_group, inodes_per_group, inode_blk_per_group, inode_size, blk_per_block, blk_per_block2;
uint32_t block_size, sec_per_blk, ext_inode, *blklst;
uint8_t desc_size, data[65536], dir[65536];
ext_inode_t *ino;

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
    ext_bg_t *bgd;
    uint32_t block_offs = ((inode - 1) / inodes_per_group) * desc_size;
    uint32_t inode_offs = ((inode - 1) % inodes_per_group) * inode_size;
    /* load the block group descriptor */
    loadblk((block_size >= 2048 ? 1 : 2) + block_offs / block_size, root_buf);
    bgd = (ext_bg_t*)(root_buf + block_offs % block_size);
    loadblk((desc_size > 32 ? (uint64_t)bgd->bg_inode_table_hi << 32UL: 0) + bgd->bg_inode_table + inode_offs / block_size, root_buf);
    /* copy inode from the table into the beginning of our buffer */
    memcpy(root_buf, root_buf + inode_offs % block_size, sizeof(ext_inode_t));
    memset(ind, 0, sizeof(ind));
    ext_inode = inode;
    ino_size = ((uint64_t)ino->i_size_hi << 32UL) | (uint64_t)ino->i_size;
}

/**
 * Read from opened file
 */
static uint64_t ext_read(uint64_t offs, uint64_t size, void *buf)
{
    uint64_t blk, rem;
    uint32_t i, j, os = 0, rs = block_size, lvl = 0;
    ext_eh_t *eh = (ext_eh_t*)ino->i_block;
    ext_ei_t *ei;
    ext_ee_t *ee;

    if(!ext_inode || offs >= ino_size || !size || !buf) return 0;
    if(offs + size > ino_size) size = ino_size - offs;
    rem = size;

    pb_init(size);
    os = offs % block_size;
    offs /= block_size;
    if(os) rs = block_size - os;
    while(rem) {
        /* convert file offset block number to file system block number */
        /* we keep indirect records cached in blklst, and the block numbers where they were loaded from in ind[X]
         *   ind[0] -> blklst[0 .. blk_per_block-1]
         *   ind[1] -> blklst[blk_per_block .. 2*blk_per_block-1]
         *   ind[2] -> blklst[2*blk_per_block .. 3*blk_per_block-1]
         * we do the same for extents, just with a different pointer on blklst */
        blk = 0;
        if(ino->i_flags & EXT4_INLINE_DATA_FL) {
            /* Inline Data */
            if((ino->i_mode & EXT2_IFMSK) == EXT2_IFDIR)
                memcpy(data, &ino->i_block[1], 56);
            else
                memcpy(data, ino->i_block, 60);
        } else
        if(ino->i_flags & EXT4_EXTENTS_FL) {
            /* Extent Tree */
again:      if(lvl < 6 && eh->eh_magic == EXT4_EH_MAGIC) {
                if(!eh->eh_depth) {
                    ee = (ext_ee_t*)&eh[1];
                    for(i = 0; !blk && i < eh->eh_entries; i++, ee++)
                        if(offs >= ee->ee_block && offs < ee->ee_block + ee->ee_len)
                            blk = (((uint64_t)ee->ee_start_hi << 32UL) | (uint64_t)ee->ee_start_lo) + offs - ee->ee_block;
                } else {
                    ei = (ext_ei_t*)&eh[1];
                    for(i = 0; i < eh->eh_entries; i++, ei++)
                        if(offs >= ei->ei_block && (i + 1 == eh->eh_entries || offs < ei[1].ei_block)) {
                            eh = (ext_eh_t*)(blklst + (lvl % 3) * block_size);
                            j = ((uint64_t)ei->ei_leaf_hi << 32UL) | (uint64_t)ei->ei_leaf_lo;
                            if(ind[lvl % 3] != j) { ind[lvl % 3] = j; loadblk(j, (uint8_t*)eh); }
                            lvl++; goto again;
                        }
                }
            }
            if(!blk) break;
        } else {
            /* Direct/Indirect Block Addressing */
            if(offs < 12) { /* direct */ blk = ino->i_block[offs]; } else
            if(offs >= 12 && offs < 12 + blk_per_block) {
                /* indirect */
                if(ind[0] != ino->i_block[12]) { ind[0] = ino->i_block[12]; loadblk(ind[0], (uint8_t*)blklst); }
                blk = blklst[offs - 12];
            } else
            if(offs >= 12 + blk_per_block && offs < 12 + blk_per_block + blk_per_block2) {
                /* double indirect */
                if(ind[0] != ino->i_block[13]) { ind[0] = ino->i_block[13]; loadblk(ind[0], (uint8_t*)blklst); }
                i = offs - 12 - blk_per_block; j = blklst[i / blk_per_block];
                if(ind[1] != j) { ind[1] = j; loadblk(ind[1], (uint8_t*)blklst + block_size); }
                blk = blklst[blk_per_block + (i % blk_per_block)];
            } else {
                /* triple indirect */
                if(ind[0] != ino->i_block[14]) { ind[0] = ino->i_block[14]; loadblk(ind[0], (uint8_t*)blklst); }
                i = offs - 12 - blk_per_block - blk_per_block2; j = blklst[i / blk_per_block2];
                if(ind[1] != j) { ind[1] = j; loadblk(ind[1], (uint8_t*)blklst + block_size); }
                j = blklst[blk_per_block + ((i / blk_per_block) % blk_per_block)];
                if(ind[2] != j) { ind[2] = j; loadblk(ind[2], (uint8_t*)blklst + 2*block_size); }
                blk = blklst[blk_per_block + blk_per_block + (i % blk_per_block)];
            }
            if(!blk) break;
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
static int ext_open(char *fn)
{
    ext_dirent_t *de;
    uint64_t offs;
    uint32_t i, rem, redir = 0;
    char *s = fn, *e;

    if(!fn || !*fn) return 0;

    file_size = 0;
again:
    loadinode(EXT2_ROOT_INO);
    for(e = s; *e && *e != '/'; e++);
    offs = 0;
    while(offs < ino_size) {
        /* read in the next block in the directory */
        if(!ext_read(offs, block_size, dir)) break;
        rem = ino_size - offs; if(rem > block_size) rem = block_size;
        offs += block_size;
        /* check filenames in directory entries
         * since H-Tree is in a single record with a rec_len that covers all index data, no special treatment required */
        for(i = 0; i < rem; i += de->rec_len) {
            de = (ext_dirent_t*)(dir + i); if(!de->rec_len) goto err;
            if(de->inode && de->name_len == e - s && !memcmp(s, (uint8_t*)de + sizeof(ext_dirent_t), de->name_len)) {
                loadinode(de->inode);
                /* symlink */
                if((ino->i_mode & EXT2_IFMSK) == EXT2_IFLNK) {
                    if(ino_size > PATH_MAX - 1) ino_size = PATH_MAX - 1;
                    if(redir >= 8 || !ext_read(0, ino_size, dir)) goto err;
                    dir[ino_size] = 0; redir++;
                    if(dir[0] == '/') memcpy(fn, dir + 1, ino_size);
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
                if(!*e) { if((ino->i_mode & EXT2_IFMSK) == EXT2_IFREG) { file_size = ino_size; return 1; } goto err; }
                /* directory and not end of path */
                if((ino->i_mode & EXT2_IFMSK) == EXT2_IFDIR) {
                    offs = 0; s = e + 1; for(e = s; *e && *e != '/'; e++);
                    break;
                } else
                /* anything else (device, fifo etc.) or file in the middle or directory at the end */
                    goto err;
            }
        }
    }
err:ext_inode = 0;
    return 0;
}

/**
 * Close the opened file
 */
static void ext_close(void)
{
    ext_inode = 0; ino_size = 0;
}

/**
 * Plugin entry point, check superblock and call sethooks
 */
PLG_API void _start(void)
{
    ext_sb_t *sb = (ext_sb_t*)root_buf;

    if(sb->s_rev_level >= 1 && (sb->s_feature_incompat & (EXT2_FEATURE_INCOMPAT_COMPRESSION | EXT2_FEATURE_INCOMPAT_META_BG |
      EXT4_FEATURE_INCOMPAT_ENCRYPT))) {
        printf("ERROR: unsupported ext4 features\n"); return;
    }

    /* save values from the superblock */
    blocks_per_group = sb->s_blocks_per_group;
    inodes_per_group = sb->s_inodes_per_group;
    desc_size = sb->s_rev_level >= 1 && sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT ? sb->s_desc_size : 32;
    block_size = 1 << (sb->s_log_block_size + 10);
    sec_per_blk = 1 << (sb->s_log_block_size + 1);
    inode_size = sb->s_rev_level >= 1 ? sb->s_inode_size : 128;
    blk_per_block = block_size / sizeof(uint32_t); blk_per_block2 = blk_per_block * blk_per_block;
    ext_inode = 0; ino = (ext_inode_t*)root_buf; blklst = (uint32_t*)(root_buf + sizeof(ext_inode_t));

    sethooks(ext_open, ext_read, ext_close);
}
