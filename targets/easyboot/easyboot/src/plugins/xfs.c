/*
 * src/plugins/xfs.c
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
 * @brief Implements the SGI eXtended File System
 * https://git.kernel.org/pub/scm/fs/xfs/xfs-documentation.git/tree/design/XFS_Filesystem_Structure
 *
 * Supported:
 * - block sizes up to 65536
 * - inode sizes up to 4096
 * - inode big cluster
 * - inode b+tree (v2/v3)
 * - inode alignment (v3)
 * - inlined data, files and short form directories (v2)
 * - inlined extents, files and directories (v2/v3)
 * - b+tree extents, files and directories (v2/v3)
 * - big extents (v5)
 * - symlinks
 */

#include "../loader.h"

EASY_PLUGIN(PLG_T_FS) {
    { 0, 4, PLG_M_CONST, { 'X', 'F', 'S', 'B' } }
};

#define be16toh(x) (((x)&0xff)<<8|((x)&0xff00)>>8)
#define be32toh(x) (((x)&0xff)<<24|((x)&0xff00)<<8|((x)&0xff0000)>>8|((x)&0xff000000)>>24)
#define be64toh(x) ((uint64_t)((x)&0xff)<<56|(uint64_t)((x)&0xff00)<<40|(uint64_t)((x)&0xff0000)<<24|\
    (uint64_t)((x)&0xff000000)<<8|(uint64_t)(((x)>>32)&0xff)<<24|(uint64_t)(((x)>>32)&0xff00)<<8|\
    (uint64_t)(((x)>>32)&0xff0000)>>8|(uint64_t)(((x)>>32)&0xff000000)>>24)

#define	XFS_INODE_BIG_CLUSTER_SIZE	8192
#define XFS_DIFLAG2_NREXT64     4
#define BMBT_EXNTFLAG_BITLEN    1
#define BMBT_STARTOFF_BITLEN    54
#define BMBT_STARTBLOCK_BITLEN  52
#define BMBT_BLOCKCOUNT_BITLEN  21
#define XFS_MASK64LO(n) (((uint64_t)1ULL << (n)) - 1)
#define	XFS_INO_TO_AGNO(i)		((i) >> agino_log)
#define	XFS_INO_TO_AGINO(i)		((i) & (uint32_t)(((uint64_t)1ULL << (agino_log)) - 1))
#define	XFS_INO_TO_AGBNO(i)	(((i) >> inopblog) & (uint32_t)(((uint64_t)1ULL << (agblklog)) - 1))
#define	XFS_INO_TO_OFFSET(i)	((int)(i) & (uint32_t)(((uint64_t)1ULL << (inopblog)) - 1))
#define	XFS_AGINO_TO_AGBNO(i)	((i) >> inopblog)
#define XFS_AGB_TO_FSB(agno,agbno)	(((uint64_t)(agno) << agblklog) | (agbno))
#define	XFS_AGB_TO_DADDR(agno,agbno)	((uint64_t)XFS_FSB_TO_BB(((uint64_t)(agno) << agblklog) + (agbno)))
#define	XFS_FSB_TO_BB(fsbno)	((fsbno) << blkbb_log)
#define	XFS_BB_TO_FSB(bb)	(((bb) + (XFS_FSB_TO_BB(1) - 1)) >> blkbb_log)
#define	XFS_BB_TO_FSBT(bb)	((bb) >> blkbb_log)
#define XFS_IFDIR              0x4000
#define XFS_IFREG              0x8000
#define XFS_IFLNK              0xA000
#define XFS_IFMSK              0xF000
#define XFS_TYPE               (di_mode&XFS_IFMSK)

typedef struct {
	uint32_t	sb_magicnum;	/* magic number == XFS_SB_MAGIC */
	uint32_t	sb_blocksize;	/* logical block size, bytes */
	uint64_t	sb_dblocks;	/* number of data blocks */
	uint64_t	sb_rblocks;	/* number of realtime blocks */
	uint64_t	sb_rextents;	/* number of realtime extents */
	uint8_t		sb_uuid[16];	/* user-visible file system unique id */
	uint64_t	sb_logstart;	/* starting block of log if internal */
	uint64_t	sb_rootino;	/* root inode number */
	uint64_t	sb_rbmino;	/* bitmap inode for realtime extents */
	uint64_t	sb_rsumino;	/* summary inode for rt bitmap */
	uint32_t	sb_rextsize;	/* realtime extent size, blocks */
	uint32_t	sb_agblocks;	/* size of an allocation group */
	uint32_t	sb_agcount;	/* number of allocation groups */
	uint32_t	sb_rbmblocks;	/* number of rt bitmap blocks */
	uint32_t	sb_logblocks;	/* number of log blocks */
	uint16_t	sb_versionnum;	/* header version == XFS_SB_VERSION */
	uint16_t	sb_sectsize;	/* volume sector size, bytes */
	uint16_t	sb_inodesize;	/* inode size, bytes */
	uint16_t	sb_inopblock;	/* inodes per block */
	char		sb_fname[12]; /* file system name */
	uint8_t		sb_blocklog;	/* log2 of sb_blocksize */
	uint8_t		sb_sectlog;	/* log2 of sb_sectsize */
	uint8_t		sb_inodelog;	/* log2 of sb_inodesize */
	uint8_t		sb_inopblog;	/* log2 of sb_inopblock */
	uint8_t		sb_agblklog;	/* log2 of sb_agblocks (rounded up) */
	uint8_t		sb_rextslog;	/* log2 of sb_rextents */
	uint8_t		sb_inprogress;	/* mkfs is in progress, don't mount */
	uint8_t		sb_imax_pct;	/* max % of fs for inode space */
					/* statistics */
	/*
	 * These fields must remain contiguous.  If you really
	 * want to change their layout, make sure you fix the
	 * code in xfs_trans_apply_sb_deltas().
	 */
	uint64_t	sb_icount;	/* allocated inodes */
	uint64_t	sb_ifree;	/* free inodes */
	uint64_t	sb_fdblocks;	/* free data blocks */
	uint64_t	sb_frextents;	/* free realtime extents */
	/*
	 * End contiguous fields.
	 */
	uint64_t	sb_uquotino;	/* user quota inode */
	uint64_t	sb_gquotino;	/* group quota inode */
	uint16_t	sb_qflags;	/* quota flags */
	uint8_t		sb_flags;	/* misc. flags */
	uint8_t		sb_shared_vn;	/* shared version number */
	uint32_t	sb_inoalignmt;	/* inode chunk alignment, fsblocks */
	uint32_t	sb_unit;	/* stripe or raid unit */
	uint32_t	sb_width;	/* stripe or raid width */
	uint8_t		sb_dirblklog;	/* log2 of dir block size (fsbs) */
	uint8_t		sb_logsectlog;	/* log2 of the log sector size */
	uint16_t	sb_logsectsize;	/* sector size for the log, bytes */
	uint32_t	sb_logsunit;	/* stripe unit size for the log */
	uint32_t	sb_features2;	/* additional feature bits */

	/*
	 * bad features2 field as a result of failing to pad the sb structure to
	 * 64 bits. Some machines will be using this field for features2 bits.
	 * Easiest just to mark it bad and not use it for anything else.
	 *
	 * This is not kept up to date in memory; it is always overwritten by
	 * the value in sb_features2 when formatting the incore superblock to
	 * the disk buffer.
	 */
	uint32_t	sb_bad_features2;

	/* version 5 superblock fields start here */

	/* feature masks */
	uint32_t	sb_features_compat;
	uint32_t	sb_features_ro_compat;
	uint32_t	sb_features_incompat;
	uint32_t	sb_features_log_incompat;

	uint32_t	sb_crc;		/* superblock crc */
	uint32_t	sb_spino_align;	/* sparse inode chunk alignment */

	uint64_t	sb_pquotino;	/* project quota inode */
	int64_t	sb_lsn;		/* last write sequence */
	uint8_t		sb_meta_uuid[16];	/* metadata file system unique id */

	/* must be padded to 64 bit alignment */
} xfs_sb_t;

typedef struct {
	/*
	 * Common allocation group header information
	 */
	uint32_t		agi_magicnum;	/* magic number == XFS_AGI_MAGIC */
	uint32_t		agi_versionnum;	/* header version == XFS_AGI_VERSION */
	uint32_t		agi_seqno;	/* sequence # starting from 0 */
	uint32_t		agi_length;	/* size in blocks of a.g. */
	/*
	 * Inode information
	 * Inodes are mapped by interpreting the inode number, so no
	 * mapping data is needed here.
	 */
	uint32_t		agi_count;	/* count of allocated inodes */
	uint32_t		agi_root;	/* root of inode btree */
	uint32_t		agi_level;	/* levels in inode btree */
	uint32_t		agi_freecount;	/* number of free inodes */

	uint32_t		agi_newino;	/* new inode just allocated */
	uint32_t		agi_dirino;	/* last directory inode chunk */
	/*
	 * Hash table of inodes which have been unlinked but are
	 * still being referenced.
	 */
	uint32_t		agi_unlinked[64];
	/*
	 * This marks the end of logging region 1 and start of logging region 2.
	 */
	uint8_t		agi_uuid[16];	/* uuid of filesystem */
	uint32_t		agi_crc;	/* crc of agi sector */
	uint32_t		agi_pad32;
	uint64_t		agi_lsn;	/* last write sequence */

	uint32_t		agi_free_root; /* root of the free inode btree */
	uint32_t		agi_free_level;/* levels in free inode btree */

	uint32_t		agi_iblocks;	/* inobt blocks used */
	uint32_t		agi_fblocks;	/* finobt blocks used */

	/* structure must be padded to 64 bit alignment */
} xfs_agi_t;

typedef struct {
	uint16_t		di_magic;	/* inode magic # = XFS_DINODE_MAGIC */
	uint16_t		di_mode;	/* mode and type of file */
	uint8_t		di_version;	/* inode version */
	uint8_t		di_format;	/* format of di_c data */
	uint16_t		di_onlink;	/* old number of links to file */
	uint32_t		di_uid;		/* owner's user id */
	uint32_t		di_gid;		/* owner's group id */
	uint32_t		di_nlink;	/* number of links to file */
	uint16_t		di_projid_lo;	/* lower part of owner's project id */
	uint16_t		di_projid_hi;	/* higher part owner's project id */
	union {
		/* Number of data fork extents if NREXT64 is set */
		uint64_t	di_big_nextents;

		/* Padding for V3 inodes without NREXT64 set. */
		uint64_t	di_v3_pad;

		/* Padding and inode flush counter for V2 inodes. */
		struct {
			uint8_t	di_v2_pad[6];
			uint16_t	di_flushiter;
		} a;
	} a;
	uint64_t	di_atime;	/* time last accessed */
	uint64_t	di_mtime;	/* time last modified */
	uint64_t	di_ctime;	/* time created/inode modified */
	uint64_t		di_size;	/* number of bytes in file */
	uint64_t		di_nblocks;	/* # of direct & btree blocks used */
	uint32_t		di_extsize;	/* basic/minimum extent size for file */
	union {
		/*
		 * For V2 inodes and V3 inodes without NREXT64 set, this
		 * is the number of data and attr fork extents.
		 */
		struct {
			uint32_t	di_nextents;
			uint16_t	di_anextents;
		} __attribute__((packed)) a;

		/* Number of attr fork extents if NREXT64 is set. */
		struct {
			uint32_t	di_big_anextents;
			uint16_t	di_nrext64_pad;
		} __attribute__((packed)) b;
	} __attribute__((packed)) b;
	uint8_t		di_forkoff;	/* attr fork offs, <<3 for 64b align */
	int8_t		di_aformat;	/* format of attr fork's data */
	uint32_t		di_dmevmask;	/* DMIG event mask */
	uint16_t		di_dmstate;	/* DMIG state info */
	uint16_t		di_flags;	/* random flags, XFS_DIFLAG_... */
	uint32_t		di_gen;		/* generation number */

	/* di_next_unlinked is the only non-core field in the old dinode */
	uint32_t		di_next_unlinked;/* agi unlinked list ptr */

	/* start of the extended dinode, writable fields */
	uint32_t		di_crc;		/* CRC of the inode */
	uint64_t		di_changecount;	/* number of attribute changes */
	uint64_t		di_lsn;		/* flush sequence */
	uint64_t		di_flags2;	/* more random flags */
	uint32_t		di_cowextsize;	/* basic cow extent size for file */
	uint8_t		di_pad2[12];	/* more padding for future expansion */

	/* fields only written to during inode creation */
	uint64_t	di_crtime;	/* time created */
	uint64_t		di_ino;		/* inode number */
	uint8_t		di_uuid[16];	/* UUID of the filesystem */

	/* structure must be padded to 64 bit alignment */
} xfs_inode_t;

typedef struct {
	uint64_t	br_startoff;	/* starting file offset */
	uint64_t	br_startblock;	/* starting block number */
	uint32_t	br_blockcount;	/* number of blocks */
} xfs_bmbt_irec_t;

typedef struct {
	uint32_t		bb_magic;	/* magic number for block type */
	uint16_t		bb_level;	/* 0 is a leaf */
	uint16_t		bb_numrecs;	/* current # of data records */
	uint32_t		bb_leftsib;
	uint32_t		bb_rightsib;
	uint64_t		bb_blkno;
	uint64_t		bb_lsn;
	uint8_t		bb_uuid[16];
	uint32_t		bb_owner;
	uint32_t		bb_crc;
} xfs_btree_sblock_t;

typedef struct {
	uint32_t		bb_magic;	/* magic number for block type */
	uint16_t		bb_level;	/* 0 is a leaf */
	uint16_t		bb_numrecs;	/* current # of data records */
	uint64_t		bb_leftsib;
	uint64_t		bb_rightsib;
	uint64_t		bb_blkno;
	uint64_t		bb_lsn;
	uint8_t		bb_uuid[16];
	uint32_t		bb_owner;
	uint32_t		bb_crc;
	uint32_t		bb_pad;
} xfs_btree_lblock_t;

typedef struct {
	uint16_t		bb_level;	/* 0 is a leaf */
	uint16_t		bb_numrecs;	/* current # of data records */
} xfs_btree_bmdr_t;

#define BTREE_KEY(keyno) (keysize == 8 ? be64toh(*((uint64_t*)(keys + (keyno - 1) * keylen))) : \
    be32toh(*((uint32_t*)(keys + (keyno - 1) * keylen))))
#define BTREE_VAL(keyno) (keysize == 8 ? be64toh(*((uint64_t*)(keys + numrecs * keylen + (keyno - 1) * ptrlen))) : \
    be32toh(*((uint32_t*)(keys + numrecs * keylen + (keyno - 1) * ptrlen))))

uint64_t xfs_inode, root_ino, inoalign_mask;
uint32_t block_size, sec_per_blk, inode_size, inode_agi, agino_log, blks_per_cluster, blkbb_log;
uint8_t inopblog, agblklog, di_u[4096];

enum { XFS_DINODE_FMT_DEV, XFS_DINODE_FMT_LOCAL, XFS_DINODE_FMT_EXTENTS, XFS_DINODE_FMT_BTREE };
uint8_t di_format;	/* format of di_u data */
uint16_t di_mode;	/* mode and type of file */
uint64_t di_size;	/* number of bytes in file */
uint64_t di_nextents;

/*
 * Convert a compressed bmap extent record to an uncompressed form.
 * This code must be in sync with the routines xfs_bmbt_get_startoff,
 * xfs_bmbt_get_startblock, xfs_bmbt_get_blockcount and xfs_bmbt_get_state.
 */
static void xfs_bmbt_get_all(uint64_t *l, xfs_bmbt_irec_t *irec)
{
	uint64_t	l0 = be64toh(l[0]);
	uint64_t	l1 = be64toh(l[1]);

	irec->br_startoff = (l0 & XFS_MASK64LO(64 - BMBT_EXNTFLAG_BITLEN)) >> 9;
	irec->br_startblock = ((l0 & XFS_MASK64LO(9)) << 43) | (l1 >> 21);
	irec->br_blockcount = l1 & XFS_MASK64LO(21);
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
 * Extent lookup
 */
static int extent_lookup(uint64_t *rec, int numext, uint64_t offs, uint64_t *out)
{
    xfs_bmbt_irec_t irec;
    int i;

    for(i = 0; i < numext; i++, rec += 2) {
        xfs_bmbt_get_all(rec, &irec);
        if(offs >= irec.br_startoff && offs < irec.br_startoff + irec.br_blockcount * block_size) {
            *out = irec.br_startblock + (offs - irec.br_startoff) / block_size;
            return 1;
        }
    }
    return 0;
}

/**
 * btree lookup
 */
static int btree_lookup(uint64_t agoff, uint64_t blk, int nlevels, int64_t key, uint64_t *out)
{
    xfs_btree_sblock_t *sbt = (xfs_btree_sblock_t *)root_buf;
    xfs_btree_lblock_t *lbt = (xfs_btree_lblock_t *)root_buf;
    uint8_t *keys;
    int64_t mkey, diff;
    int level, low, high, keyno = 0;
    uint16_t numrecs, keylen, keysize, ptrlen;

    for (level = nlevels - 1, diff = 1; level >= 0; level--) {
        /* load the block */
        if(blk != -1ULL)
            loadblk(agoff + blk, root_buf);
        /* set up b+tree search */
        if(blk == -1ULL) {
            /* it's a BMAP embedded in the inode */
            numrecs = be16toh(((xfs_btree_bmdr_t*)di_u)->bb_numrecs);
            keylen = keysize = ptrlen = 8;
            keys = di_u + sizeof(xfs_btree_bmdr_t);
            if(!nlevels) return extent_lookup((uint64_t*)keys, numrecs, key, out);
        } else
        if(!memcmp(root_buf, "IAB3", 4) || !memcmp(root_buf, "IABT", 4)) {
            /* inode tree */
            numrecs = be16toh(sbt->bb_numrecs);
            keylen = sbt->bb_level ? sizeof(uint32_t) : 16;
            keysize = ptrlen = 4;
            keys = root_buf + sizeof(xfs_btree_sblock_t);
        } else
        if(!memcmp(root_buf, "BMA3", 4) || !memcmp(root_buf, "BMAP", 4)) {
            /* extent tree */
            numrecs = be16toh(lbt->bb_numrecs);
            keylen = keysize = ptrlen = 8;
            keys = root_buf + sizeof(xfs_btree_lblock_t);
            if(!lbt->bb_level) return extent_lookup((uint64_t*)keys, numrecs, key, out);
        } else return 0;
        if(!diff) keyno = 1;
        else {
            low = 1; high = numrecs;
            while (low <= high) {
                keyno = (low + high) >> 1;
                mkey = BTREE_KEY(keyno);
                diff = mkey - key;
                if (diff < 0) low = keyno + 1; else if (diff > 0) high = keyno - 1; else break;
            }
        }
        if (level > 0) {
            if (diff > 0 && --keyno < 1)
                keyno = 1;
            blk = BTREE_VAL(keyno);
        } else
        if(!level && !diff)
            *out = BTREE_VAL(keyno);
    }
    return !diff && keyno;
}

/**
 * Load an inode
 */
static void loadinode(uint64_t inode)
{
    xfs_agi_t *agi = (xfs_agi_t*)root_buf;
    xfs_inode_t *ino;
    uint64_t i, agno, agino, agbno, bno, off, len, offset_agbno, cluster_agbno, chunk_agbno, chunk_agino;

    xfs_inode = 0;

    /* figure out inode's location on disk */
    agno = XFS_INO_TO_AGNO(inode);
    agino = XFS_INO_TO_AGINO(inode);
    agbno = XFS_AGINO_TO_AGBNO(agino);
    off = XFS_INO_TO_OFFSET(inode);
    len = 1;
    if(block_size >= XFS_INODE_BIG_CLUSTER_SIZE) {
        bno = XFS_AGB_TO_FSB(agno, agbno);
    } else {
        if(inoalign_mask) {
            offset_agbno = agbno & inoalign_mask;
            chunk_agbno = agbno - offset_agbno;
        } else {
            loadsec(((agno << agblklog) + inode_agi) >> 9, root_buf);
            if(memcmp(root_buf, "XAGI", 4) ||
                !btree_lookup(agno << agblklog, be32toh(agi->agi_root), be32toh(agi->agi_level), agino, &chunk_agino)) return;
            chunk_agbno = XFS_AGINO_TO_AGBNO(chunk_agino);
            offset_agbno = agbno - chunk_agbno;
        }
        cluster_agbno = chunk_agbno + ((offset_agbno / blks_per_cluster) * blks_per_cluster);
        off += ((agbno - cluster_agbno) << inopblog);
        bno = XFS_AGB_TO_FSB(agno, cluster_agbno);
        len = blks_per_cluster;
    }

    /* load inode */
    for(i = 0; i < len; i++)
        loadblk((agno << agblklog) + bno + i, root_buf + i * block_size);

    /* fill in in-memory data */
    ino = (xfs_inode_t*)(root_buf + off);
    if(!memcmp(&ino->di_magic, "IN", 2)) {
        di_format = ino->di_format;
        di_mode = be16toh(ino->di_mode);
        di_size = be64toh(ino->di_size);
        di_nextents = ino->di_version >= 3 && (be64toh(ino->di_flags2) & XFS_DIFLAG2_NREXT64)
            ? be64toh(ino->a.di_big_nextents) : be32toh(ino->b.a.di_nextents);
        i = (ino->di_version >= 3 ? 176 : 100);
        memcpy(di_u, root_buf + off + i, inode_size - i);
        xfs_inode = inode;
    }
}

/**
 * Read from opened file
 */
static uint64_t xfs_read(uint64_t offs, uint64_t size, void *buf)
{
    uint64_t rem, blk;
    uint32_t os = 0, rs = block_size;

    if(!xfs_inode || offs >= di_size || !size || !buf) return 0;
    if(offs + size > di_size) size = di_size - offs;
    rem = size;

    pb_init(size);
    os = offs % block_size;
    offs /= block_size;
    if(os) rs = block_size - os;
    while(rem) {
        /* convert file offset block number to file system block number */
        blk = 0;
        if(rs > rem) rs = rem;
        if(di_format == XFS_DINODE_FMT_LOCAL) { memcpy(buf, di_u + os, rs); blk = 1; } else
        if(((di_format == XFS_DINODE_FMT_EXTENTS && extent_lookup((uint64_t*)di_u, di_nextents, offs * block_size, &blk)) ||
            (di_format == XFS_DINODE_FMT_BTREE && btree_lookup(0, -1ULL, be16toh(((xfs_btree_bmdr_t*)di_u)->bb_level), offs * block_size, &blk))) &&
            blk) {
                loadblk(blk, root_buf);
                memcpy(buf, root_buf + os, rs);
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
static int xfs_open(char *fn)
{
    xfs_btree_bmdr_t *bmdr = (xfs_btree_bmdr_t*)di_u;
    uint64_t offs, rem, blk, prev;
    uint32_t i, redir = 0;
    uint8_t *d = root_buf, *end, *n, l, fmt = 0;
    char *s = fn, *e;

    if(!fn || !*fn) return 0;

    file_size = 0;
again:
    loadinode(root_ino);
    for(e = s; *e && *e != '/'; e++);
    offs = prev = 0; fmt = 0;
    while(offs < di_size) {
        /* unfortunately we can't use xfs_read() here like we do with all the other file systems... */
        rem = offs & ~(block_size - 1);
        if(di_format == XFS_DINODE_FMT_LOCAL) {
            if(di_u[1]) { d = di_u + 10; fmt = 2; }
            else { d = di_u + 6; fmt = 1; }
        } else
        if(((di_format == XFS_DINODE_FMT_EXTENTS && extent_lookup((uint64_t*)di_u, 1, rem, &blk)) ||
            (di_format == XFS_DINODE_FMT_BTREE && btree_lookup(0, -1ULL, be16toh(bmdr->bb_level), rem, &blk))) &&
            blk) {
                loadblk(blk, root_buf + prev);
                d = root_buf + (prev ? 0 : offs - rem);
                prev = 0;
                if(!offs) {
                    if(!memcmp(d, "XD2B", 4)) { d += 16; offs += 16; } else
                    if(!memcmp(d, "XDB3", 4)) { d += 64; offs += 64; } else goto err;
                }
                fmt = 3;
        }
        if(!fmt) break;
        rem = di_size - offs; if(rem > block_size) rem = block_size;
        end = d + rem;
        /* check filenames in directory entries */
        while(d < end) {
            blk = 0; l = 0; n = NULL;
            if(fmt < 3) {
                /* short form always inlined in inode, we surely have the entire directory loaded */
                l = *d++; d += 2;
                n = d; d += l + 1;
                blk = fmt == 2 ? be64toh(*((uint64_t*)d)) : be32toh(*((uint64_t*)d));
                d += fmt * 4;
                offs += (fmt + 1) * 4 + l;
            } else {
                /* if directory entry crosses block boundary */
                if(d + 12 + 256 > end) {
                    prev = end - d;
                    memcpy(root_buf, d, prev);
                    offs += prev;
                    break;
                }
                blk = be64toh(*((uint64_t*)d)); d += 8;
                l = *d++;
                n = d; d += l + 1;
                d += 2;
                offs += 12 + l;
            }
            if(blk && n && e - s == l && !memcmp(s, n, l)) {
                loadinode(blk);
                /* symlink */
                if(XFS_TYPE == XFS_IFLNK) {
                    i = di_size;
                    if(i > PATH_MAX - 1) i = PATH_MAX - 1;
                    if(redir >= 8 || !xfs_read(0, i, root_buf)) goto err;
                    root_buf[i] = 0; redir++;
                    if(root_buf[0] == '/') memcpy(fn, root_buf + 1, i);
                    else {
                        for(e = (char*)root_buf; *e && s < &fn[PATH_MAX - 1]; e++) {
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
                if(!*e) { if(XFS_TYPE == XFS_IFREG) { file_size = di_size; return 1; } goto err; }
                /* directory and not end of path */
                if(XFS_TYPE == XFS_IFDIR) {
                    offs = prev = 0; fmt = 0; s = e + 1; for(e = s; *e && *e != '/'; e++);
                    break;
                } else
                /* anything else (device, fifo etc.) or file in the middle or directory at the end */
                    goto err;
            }
        }
    }
err:xfs_inode = 0;
    return 0;
}

/**
 * Close the opened file
 */
static void xfs_close(void)
{
    xfs_inode = 0;
}

/**
 * Plugin entry point, check superblock and call sethooks
 */
PLG_API void _start(void)
{
    xfs_sb_t *sb = (xfs_sb_t*)root_buf;
    uint32_t v;

    /* save values from the superblock */
    block_size = be32toh(sb->sb_blocksize);
    inode_size = be16toh(sb->sb_inodesize);
    inode_agi = 2*be16toh(sb->sb_sectsize);
    sec_per_blk = block_size >> 9;
    root_ino = be64toh(sb->sb_rootino);
    inopblog = sb->sb_inopblog;
    agblklog = sb->sb_agblklog;
    agino_log = sb->sb_inopblog + sb->sb_agblklog;
    blkbb_log = sb->sb_blocklog - 9;
    blks_per_cluster = XFS_INODE_BIG_CLUSTER_SIZE / block_size;
    v = be32toh(sb->sb_versionnum);
    inoalign_mask = (v&0xf) >= 4 && (v&0x80) && be32toh(sb->sb_inoalignmt) >= blks_per_cluster ? be32toh(sb->sb_inoalignmt) - 1 : 0;
    xfs_inode = 0;
    if(block_size > 65536 || inode_size < 512 || inode_size > 4096 || inode_agi > 65536 || (inode_agi & 511) || !root_ino) return;

    sethooks(xfs_open, xfs_read, xfs_close);
}
