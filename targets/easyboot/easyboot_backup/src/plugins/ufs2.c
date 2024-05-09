/*
 * src/plugins/ufs2.c
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
 * @brief Implements the UNIX filesystem v2 (FreeBSD variant)
 * https://alter.org.ua/docs/fbsd/ufs/
 * (almost the same as ufs44.c, but different sb position and magic)
 */

#include "../loader.h"

#ifdef UFS44
#define SBPOS 8192
EASY_PLUGIN(PLG_T_FS) {
    { 0x255c, 4, PLG_M_CONST, { 0x54, 0x19, 0x01, 0x00 } }
};
#else
#define SBPOS 65536
EASY_PLUGIN(PLG_T_FS) {
    { 0x55c, 4, PLG_M_CONST, { 0x19, 0x01, 0x54, 0x19 } }
};
#endif

#define UFS_ROOT_INO    2       /* Root inode */

/* di_mode */
#define UFS_IFDIR              0x4000
#define UFS_IFREG              0x8000
#define UFS_IFLNK              0xA000
#define UFS_IFMSK              0xF000

typedef struct {
	int32_t	 fs_firstfield;		/* historic filesystem linked list, */
	int32_t	 fs_unused_1;		/*     used for incore super blocks */
	int32_t	 fs_sblkno;		/* offset of super-block in filesys */
	int32_t	 fs_cblkno;		/* offset of cyl-block in filesys */
	int32_t	 fs_iblkno;		/* offset of inode-blocks in filesys */
	int32_t	 fs_dblkno;		/* offset of first data after cg */
	int32_t	 fs_old_cgoffset;	/* cylinder group offset in cylinder */
	int32_t	 fs_old_cgmask;		/* used to calc mod fs_ntrak */
	int32_t  fs_old_time;		/* last time written */
	int32_t	 fs_old_size;		/* number of blocks in fs */
	int32_t	 fs_old_dsize;		/* number of data blocks in fs */
	uint32_t fs_ncg;		/* number of cylinder groups */
	int32_t	 fs_bsize;		/* size of basic blocks in fs */
	int32_t	 fs_fsize;		/* size of frag blocks in fs */
	int32_t	 fs_frag;		/* number of frags in a block in fs */
/* these are configuration parameters */
	int32_t	 fs_minfree;		/* minimum percentage of free blocks */
	int32_t	 fs_old_rotdelay;	/* num of ms for optimal next block */
	int32_t	 fs_old_rps;		/* disk revolutions per second */
/* these fields can be computed from the others */
	int32_t	 fs_bmask;		/* ``blkoff'' calc of blk offsets */
	int32_t	 fs_fmask;		/* ``fragoff'' calc of frag offsets */
	int32_t	 fs_bshift;		/* ``lblkno'' calc of logical blkno */
	int32_t	 fs_fshift;		/* ``numfrags'' calc number of frags */
/* these are configuration parameters */
	int32_t	 fs_maxcontig;		/* max number of contiguous blks */
	int32_t	 fs_maxbpg;		/* max number of blks per cyl group */
/* these fields can be computed from the others */
	int32_t	 fs_fragshift;		/* block to frag shift */
	int32_t	 fs_fsbtodb;		/* fsbtodb and dbtofsb shift constant */
	int32_t	 fs_sbsize;		/* actual size of super block */
    uint8_t  fs_unused0[76];
	uint32_t fs_ipg;		/* inodes per group */
	int32_t	 fs_fpg;		/* blocks per group * fs_frag */
    uint8_t  fs_unused1[1180];
	int32_t	 fs_magic;		/* magic number */
} __attribute__((packed)) ufs2_sb_t;

#define	UFS_NXADDR	2		/* External addresses in inode. */
#define	UFS_NDADDR	12		/* Direct addresses in inode. */
#define	UFS_NIADDR	3		/* Indirect addresses in inode. */
typedef struct {
	uint16_t	di_mode;	/*   0: IFMT, permissions; see below. */
	int16_t		di_nlink;	/*   2: File link count. */
	uint32_t	di_uid;		/*   4: File owner. */
	uint32_t	di_gid;		/*   8: File group. */
	uint32_t	di_blksize;	/*  12: Inode blocksize. */
	uint64_t	di_size;	/*  16: File byte count. */
	uint64_t	di_blocks;	/*  24: Blocks actually held. */
	uint64_t	di_atime;	/*  32: Last access time. */
	uint64_t	di_mtime;	/*  40: Last modified time. */
	uint64_t	di_ctime;	/*  48: Last inode change time. */
	uint64_t	di_birthtime;	/*  56: Inode creation time. */
	int32_t		di_mtimensec;	/*  64: Last modified time. */
	int32_t		di_atimensec;	/*  68: Last access time. */
	int32_t		di_ctimensec;	/*  72: Last inode change time. */
	int32_t		di_birthnsec;	/*  76: Inode creation time. */
	uint32_t	di_gen;		/*  80: Generation number. */
	uint32_t	di_kernflags;	/*  84: Kernel flags. */
	uint32_t	di_flags;	/*  88: Status flags (chflags). */
	uint32_t	di_extsize;	/*  92: External attributes size. */
	uint64_t	di_extb[UFS_NXADDR];/* 96: External attributes block. */
	union {
		struct {
			uint64_t	di_db /* 112: Direct disk blocks. */
			    [UFS_NDADDR];
			uint64_t	di_ib /* 208: Indirect disk blocks. */
			    [UFS_NIADDR];
		};
		char	di_shortlink	/* 112: Embedded symbolic link. */
		    [(UFS_NDADDR + UFS_NIADDR) * sizeof(uint64_t)];
	};
	uint64_t	di_modrev;	/* 232: i_modrev for NFSv4 */
	union {
		uint32_t di_freelink;	/* 240: SUJ: Next unlinked inode. */
		uint32_t di_dirdepth;	/* 240: IFDIR: depth from root dir */
	};
	uint32_t	di_ckhash;	/* 244: if CK_INODE, its check-hash */
	uint32_t	di_spare[2];	/* 248: Reserved; currently unused */
} __attribute__((packed)) ufs2_dinode_t;

typedef struct {
	uint32_t	d_ino;
	int16_t		d_reclen;
	uint8_t	d_type;
	uint8_t	d_namlen;
	uint8_t	d_name[4];
} __attribute__((packed)) ufs2_dirent_t;

uint64_t ufs_inode, ni1_siz, ni2_siz, ind[2], *blklst, blk_per_block, blk_per_block2;
uint32_t block_size, sec_per_blk, fs_iblkno, fs_ipg, fs_fpg;
uint8_t data[32768], dir[32768+256+sizeof(ufs2_dirent_t)];
ufs2_dinode_t *ino;

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
    uint32_t cg = inode / fs_ipg;
    uint32_t cgino = inode % fs_ipg;
    uint32_t block_offs = (cgino << 8) / block_size;
    uint32_t inode_offs = (cgino << 8) % block_size;

    loadblk((cg * fs_fpg) + fs_iblkno + block_offs, root_buf);
    memcpy(root_buf, root_buf + inode_offs, sizeof(ufs2_dinode_t));
    memset(ind, 0, sizeof(ind));
    ufs_inode = inode;
}

/**
 * Read from opened file
 */
static uint64_t ufs_read(uint64_t offs, uint64_t size, void *buf)
{
    uint64_t blk, rem;
    uint32_t i, j, k, os = 0, rs = block_size;

    if(!ufs_inode || offs >= ino->di_size || !size || !buf) return 0;
    if(offs + size > ino->di_size) size = ino->di_size - offs;
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
        if(offs < UFS_NDADDR) { /* direct */ blk = ino->di_db[offs]; } else
        if(offs >= UFS_NDADDR && offs < ni1_siz) {
            /* indirect */
            if(ind[0] != ino->di_ib[0]) { ind[0] = ino->di_ib[0]; loadblk(ind[0], (uint8_t*)blklst); }
            blk = blklst[offs - UFS_NDADDR];
        } else
        if(offs >= ni1_siz && offs < ni2_siz) {
            /* double indirect */
            i = offs - ni1_siz;
            k = i / blk_per_block2;
            if(k < UFS_NIADDR) {
                if(ind[0] != ino->di_ib[k]) { ind[0] = ino->di_ib[k]; loadblk(ind[0], (uint8_t*)blklst); }
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
static int ufs_open(char *fn)
{
    ufs2_dirent_t *de;
    uint64_t offs, prev;
    uint32_t i, rem, redir = 0;
    char *s = fn, *e;

    if(!fn || !*fn) return 0;

    file_size = 0;
again:
    loadinode(UFS_ROOT_INO);
    for(e = s; *e && *e != '/'; e++);
    offs = prev = 0;
    while(offs < ino->di_size) {
        /* read in the next block in the directory */
        if(!ufs_read(offs, block_size, root_buf + 3 * block_size)) break;
        memcpy(dir + prev, root_buf + 3 * block_size, block_size);
        prev = 0;
        rem = ino->di_size - offs; if(rem > block_size) rem = block_size;
        /* check filenames in directory entries */
        for(i = 0; i < rem; ) {
            if(i + 256 + sizeof(ufs2_dirent_t) > rem) {
                prev = rem - i;
                memcpy(dir, dir + i, prev);
                offs += prev;
                break;
            }
            de = (ufs2_dirent_t*)(dir + i);
            i += de->d_reclen;
            offs += de->d_reclen;
            if(!de->d_reclen) goto err;
            if(de->d_ino && e - s == de->d_namlen && !memcmp(s, de->d_name, e - s)) {
                loadinode(de->d_ino);
                /* symlink */
                if((ino->di_mode & UFS_IFMSK) == UFS_IFLNK) {
                    i = ino->di_size;
                    if(i > PATH_MAX - 1) i = PATH_MAX - 1;
                    if(redir >= 8 || !ufs_read(0, i, dir)) goto err;
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
                if(!*e) { if((ino->di_mode & UFS_IFMSK) == UFS_IFREG) { file_size = ino->di_size; return 1; } goto err; }
                /* directory and not end of path */
                if((ino->di_mode & UFS_IFMSK) == UFS_IFDIR) {
                    offs = prev = 0; s = e + 1; for(e = s; *e && *e != '/'; e++);
                    break;
                } else
                /* anything else (device, fifo etc.) or file in the middle or directory at the end */
                    goto err;
            }
        }
    }
err:ufs_inode = 0;
    return 0;
}

/**
 * Close the opened file
 */
static void ufs_close(void)
{
    ufs_inode = 0;
}

/**
 * Plugin entry point, check superblock and call sethooks
 */
PLG_API void _start(void)
{
    ufs2_sb_t *sb = (ufs2_sb_t*)(root_buf + SBPOS);

    if(sb->fs_fsize < 512 || sb->fs_fsize > 32768 || sb->fs_fsize & 511 || !sb->fs_ipg) return;

    /* save values from the superblock */
    block_size = sb->fs_fsize;
    fs_ipg = sb->fs_ipg;
    fs_fpg = sb->fs_fpg;
    fs_iblkno = sb->fs_iblkno;
    sec_per_blk = block_size >> 9;
    blk_per_block = block_size / sizeof(uint32_t); blk_per_block2 = blk_per_block * blk_per_block;
    ni1_siz = UFS_NDADDR + blk_per_block;
    ni2_siz = ni1_siz + blk_per_block2;
    ufs_inode = 0; ino = (ufs2_dinode_t*)root_buf; blklst = (uint64_t*)(root_buf + sizeof(ufs2_dinode_t));

    sethooks(ufs_open, ufs_read, ufs_close);
}
