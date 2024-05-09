/*
 * src/plugins/ntfs.c
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
 * @brief Implements the NTFS filesystem
 * https://en.wikipedia.org/wiki/NTFS
 *
 * Not supported:
 * - index (uses MFT exclusively)
 * - reparse points
 * - compression
 * - encryption
 */

#include "../loader.h"

EASY_PLUGIN(PLG_T_FS) {
    { 3, 4, PLG_M_CONST, { 'N', 'T', 'F', 'S' } },
    { 7, 4, PLG_M_CONST, { ' ', ' ', ' ', ' ' } }
};

/* structs originally from: https://github.com/libyal/libfsntfs/blob/main/libfsntfs */
typedef struct {
    uint8_t  boot_entry_point[ 3 ];
    uint8_t  file_system_signature[ 8 ];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster_block;
    uint8_t  unknown1[ 7 ];
    uint8_t  media_descriptor;
    uint8_t  unknown2[ 2 ];
    uint16_t sectors_per_track;
    uint16_t number_of_heads;
    uint32_t number_of_hidden_sectors;
    uint8_t  unknown3[ 4 ];
    uint8_t  unknown4[ 4 ];
    uint64_t total_number_of_sectors;
    uint64_t mft_cluster_block_number;
    uint64_t mirror_mft_cluster_block_number;
    int8_t   mft_entry_size;
    uint8_t  unknown5[ 3 ];
    int8_t   index_entry_size;
    uint8_t  unknown6[ 3 ];
    uint8_t  volume_serial_number[ 8 ];
    uint32_t checksum;
    uint8_t  bootcode[ 426 ];
    uint8_t sector_signature[ 2 ];
} __attribute__((packed)) ntfs_superblock_t;

typedef struct {
    uint8_t  signature[ 4 ]; /* "FILE" */
    uint16_t fixup_values_offset;
    uint16_t number_of_fixup_values;
    uint64_t journal_sequence_number;
    uint16_t sequence;
    uint16_t reference_count;
    uint16_t attributes_offset;
    uint16_t flags;
    uint32_t used_entry_size;
    uint32_t total_entry_size;
    uint64_t base_record_file_reference;
    uint16_t first_available_attribute_identifier;
    uint8_t  unknown1[ 2 ];
    uint32_t index;
} __attribute__((packed)) ntfs_mftent_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint8_t  non_resident_flag; /* 0 */
    uint8_t  name_size;
    uint16_t name_offset;
    uint16_t data_flags;
    uint16_t identifier;
    uint32_t data_size;
    uint32_t data_offset;
    uint8_t  indexed_flag;
    uint8_t  reserved;
} __attribute__((packed)) ntfs_attr_res_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint8_t  non_resident_flag; /* 1 */
    uint8_t  name_size;
    uint16_t name_offset;
    uint16_t data_flags;
    uint16_t identifier;
    uint64_t data_first_vcn;
    uint64_t data_last_vcn;
    uint16_t data_runs_offset;
    uint16_t compression_unit_size;
    uint32_t padding;
    uint64_t allocated_data_size;
    uint64_t data_size;
    uint64_t valid_data_size;
    uint64_t total_allocated_size; /* if compression_unit_size > 0 */
} __attribute__((packed)) ntfs_attr_nonres_t;

typedef struct {
    uint32_t type;              /* 0x30 */
    uint32_t size;
    uint8_t  non_resident_flag; /* 0 */
    uint8_t  name_size;
    uint16_t name_offset;
    uint16_t data_flags;
    uint16_t identifier;
    uint32_t data_size;
    uint32_t data_offset;
    uint8_t  indexed_flag;
    uint8_t  reserved;
    uint32_t parent;
    uint16_t parent_seq;
    uint64_t creation_time;
    uint64_t modification_time;
    uint64_t entry_modification_time;
    uint64_t access_time;
    uint64_t allocated_data_size;
    uint64_t file_size;
    uint32_t file_attribute_flags;
    uint32_t extended_data;
    uint8_t  file_name_size;
    uint8_t  file_name_space;
} __attribute__((packed)) ntfs_file_name_t;

#define MFT_ROOT 5
#define FILE_ATTRIBUTE_DIRECTORY 0x10

uint64_t clu_size, mft_clu, ntfs_fid;
uint32_t sec_size, sec_per_clu, mft_entsize, idx_entsize;
uint16_t wcname[PATH_MAX];
uint8_t mftrun[1024];

/**
 * Decode one run
 */
static uint8_t *rundec(uint8_t *ptr, uint64_t *clu, uint64_t *size)
{
    uint8_t hdr = *ptr++;
    uint64_t c;

    if(!hdr) return NULL;
    switch(hdr >> 4) {
        case 1: *size = *((int8_t*)ptr); ptr++; break;
        case 2: *size = *((int16_t*)ptr); ptr += 2; break;
        case 4: *size = *((int32_t*)ptr); ptr += 4; break;
        case 8: *size = *((int64_t*)ptr); ptr += 8; break;
        default: return NULL;
    }
    switch(hdr & 0xF) {
        case 1: c = *((int8_t*)ptr); ptr++; break;
        case 2: c = *((int16_t*)ptr); ptr += 2; break;
        case 4: c = *((int32_t*)ptr); ptr += 4; break;
        case 8: c = *((int64_t*)ptr); ptr += 8; break;
        default: return NULL;
    }
    if(!*clu) *clu = c; else (*clu) += c;
    return ptr;
}

/**
 * Fix-up MFT or INDX record (note, also aligns with ntfs_dirent_t)
 */
static void fixup(uint8_t *mft)
{
    ntfs_mftent_t *mftent = (ntfs_mftent_t *)mft;
    uint16_t i, *fixup = (uint16_t*)(mft + mftent->fixup_values_offset + 2);

    for(i = 1; i < mftent->number_of_fixup_values && i * sec_size <= mftent->used_entry_size; i++, fixup++)
        *((uint16_t*)(mft + i * sec_size - 2)) = *fixup;
}

/**
 * Load a cluster
 */
static void loadclu(uint64_t clu, uint8_t *dst)
{
    uint32_t i;
    clu *= sec_per_clu;
    /*clu += sec_hid; this is a parittion sector LBA, the correct partition_lba will be added in loadsec */
    for(i = 0; i < sec_per_clu; i++, clu++, dst += 512)
        loadsec(clu, dst);
}

/**
 * Read from opened file
 */
static uint64_t ntfs_read(uint64_t offs, uint64_t size, void *buf)
{
    uint64_t clu, c, s, o = 0, rem;
    uint32_t os = 0, rs = clu_size;
    uint8_t *p;

    if(!ntfs_fid || offs >= file_size || !size || !buf) return 0;
    if(offs + size > file_size) size = file_size - offs;
    rem = size;

    pb_init(size);
    os = offs % clu_size;
    offs /= clu_size;
    if(os) rs = clu_size - os;
    while(rem) {
        /* convert file offset cluster number to file system cluster number */
        clu = 0;
        if(rs > rem) rs = rem;
        /* $DATA stream */
        if(((ntfs_attr_nonres_t*)root_buf)->type == 0x80) {
            if(((ntfs_attr_nonres_t*)root_buf)->non_resident_flag) {
                /* cluster runs */
                for(p = root_buf + ((ntfs_attr_nonres_t*)root_buf)->data_runs_offset, c = 0; *p; o += s) {
                    if(!(p = rundec(p, &c, &s))) break;
                    if(offs >= o && offs < o + s) {
                        clu = c + offs - o;
                        loadclu(clu, root_buf + clu_size);
                        memcpy(buf, root_buf + clu_size + os, rs);
                        break;
                    }
                }
            } else {
                /* inlined data */
                memcpy(buf, root_buf + ((ntfs_attr_res_t*)root_buf)->data_offset + os, rs);
                clu = 1;
            }
        }
        if(!clu) break;
        os = 0; buf += rs; rem -= rs; rs = clu_size;
        pb_draw(size - rem);
        offs++;
    }
    pb_fini();
    return (size - rem);
}

/**
 * Look up directories and return data stream and set file_size
 */
static int ntfs_open(char *fn)
{
    uint64_t fid, parent = MFT_ROOT, i, c, l;
    uint16_t *s = wcname, *d, *e;
    uint8_t *ptr, *end, *m, *a;
    char *p = fn;

    if(!fn || !*fn) return 0;
    /* UTF-8 to WCHAR */
    for(p = fn, d = wcname; *p && d < &wcname[PATH_MAX - 2]; d++) {
        c = *p;
        if((*p & 128) != 0) {
            if(!(*p & 32)) { c = ((*p & 0x1F)<<6)|(*(p+1) & 0x3F); p += 1; } else
            if(!(*p & 16)) { c = ((*p & 0xF)<<12)|((*(p+1) & 0x3F)<<6)|(*(p+2) & 0x3F); p += 2; } else
            if(!(*p & 8)) { c = ((*p & 0x7)<<18)|((*(p+1) & 0x3F)<<12)|((*(p+2) & 0x3F)<<6)|(*(p+3) & 0x3F); p += 3; }
            else break;
        }
        p++;
        *d = c;
    }
    *d = 0;

    file_size = 0;
again:
    for(e = s; *e && *e != L'/'; e++);
    fid = 0;
    for(ptr = mftrun, c = 0; *ptr;) {
        if(!(ptr = rundec(ptr, &c, &l)) || !l) goto err;
        for(i = 0; i < l; i++) {
            loadclu(c + i, root_buf);
            for(m = root_buf; m < root_buf + clu_size && !memcmp(m, "FILE", 4); m += mft_entsize, fid++) {
                fixup(m); end = m + ((ntfs_mftent_t *)m)->used_entry_size;
                /* find the $FILE_NAME attibute and see if its parent and name match with the one we're looking for */
                for(a = m + ((ntfs_mftent_t *)m)->attributes_offset; a < end && ((ntfs_attr_res_t*)a)->size;
                  a += ((ntfs_attr_res_t*)a)->size)
                    if(((ntfs_attr_res_t*)a)->type == 0x30 && !((ntfs_attr_res_t*)a)->non_resident_flag &&
                      ((ntfs_file_name_t*)a)->parent == parent && ((ntfs_file_name_t*)a)->file_name_size == e - s &&
                      !memcmp(a + sizeof(ntfs_file_name_t), s, (e - s) << 1)) {
                        /* regular file and end of path */
                        if(!*e) {
                            if(!(((ntfs_file_name_t*)a)->file_attribute_flags & FILE_ATTRIBUTE_DIRECTORY)) {
                                file_size = ((ntfs_file_name_t*)a)->file_size;
                                /* find the $DATA attibute */
                                for(a = m + ((ntfs_mftent_t *)m)->attributes_offset; a < end && ((ntfs_attr_res_t*)a)->size;
                                  a += ((ntfs_attr_res_t*)a)->size)
                                    if(((ntfs_attr_nonres_t*)a)->type == 0x80) {
                                        memcpy(root_buf, a, ((ntfs_attr_res_t*)a)->size);
                                        ntfs_fid = fid;
                                        return 1;
                                    }
                                goto err;
                            }
                            goto err;
                        } else
                        /* directory and not end of path */
                        if((((ntfs_file_name_t*)a)->file_attribute_flags & FILE_ATTRIBUTE_DIRECTORY)) {
                            parent = fid; s = e + 1; for(e = s; *e && *e != '/'; e++);
                            goto again;
                        } else
                            /* anything else (volume label, reparse point etc.) or file in the middle or directory at the end */
                            goto err;
                    }
            }
        }
    }
err:ntfs_fid = 0;
    return 0;
}

/**
 * Close the opened file
 */
static void ntfs_close(void)
{
    ntfs_fid = 0;
}

/**
 * Plugin entry point, check superblock and call sethooks
 */
PLG_API void _start(void)
{
    ntfs_superblock_t *sb = (ntfs_superblock_t*)root_buf;
    uint8_t *ptr, *end, *p = NULL;
    uint64_t c = 0, s;

    sec_size = sb->bytes_per_sector;
    clu_size = sec_size * sb->sectors_per_cluster_block;
    if(clu_size > 65536) return;
    sec_per_clu = clu_size >> 9;
    mft_clu = sb->mft_cluster_block_number;
    mft_entsize = sb->mft_entry_size > 0 ? (uint8_t)sb->mft_entry_size * clu_size : 1U << -sb->mft_entry_size;
    idx_entsize = sb->index_entry_size > 0 ? (uint8_t)sb->index_entry_size * clu_size : 1U << -sb->index_entry_size;
    if(mft_entsize > 65536) return;
    /* load the first cluster of the MFT */
    loadclu(mft_clu, root_buf); fixup(root_buf);
    /* locate the clusters where the MFT is stored (MFT records itself in its first MFT record) */
    end = root_buf + ((ntfs_mftent_t *)root_buf)->used_entry_size;
    /* find the $DATA attibute, must be non resident for $MFT */
    for(ptr = root_buf + ((ntfs_mftent_t *)root_buf)->attributes_offset; ptr < end; ptr += ((ntfs_attr_res_t*)ptr)->size)
        if(((ntfs_attr_nonres_t*)ptr)->type == 0x80 && ((ntfs_attr_nonres_t*)ptr)->non_resident_flag) {
            ptr += ((ntfs_attr_nonres_t*)ptr)->data_runs_offset;
            for(p = ptr; p < end && *p; p += (p[0] >> 4) + (p[0] & 0xF) + 1);
            p++; if(p > end || p - ptr > (int)sizeof(mftrun)) return;
            memcpy(mftrun, ptr, p - ptr);
            break;
        }
    if(!p || !rundec(mftrun, &c, &s) || c != mft_clu || !s) return;
    ntfs_fid = 0;
    sethooks(ntfs_open, ntfs_read, ntfs_close);
}
