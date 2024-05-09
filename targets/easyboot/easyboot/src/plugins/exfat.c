/*
 * src/plugins/exfat.c
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
 * @brief Implements the exFAT filesystems
 * https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification
 */

#include "../loader.h"

EASY_PLUGIN(PLG_T_FS) {
    { 3, 4, PLG_M_CONST, { 'E', 'X', 'F', 'A' } },
    { 7, 1, PLG_M_CONST, { 'T', 0, 0, 0 } },
};

typedef struct {
  uint8_t   jmp[3];
  char      oem[8];
  uint8_t   res0[53];
  uint64_t  po;
  uint64_t  ts;
  uint32_t  fo;
  uint32_t  spf;
  uint32_t  co;
  uint32_t  cc;
  uint32_t  rc;
  uint32_t  ser;
  uint16_t  rev;
  uint16_t  flags;
  uint8_t   bps;
  uint8_t   spc;
  uint8_t   nf;
} __attribute__((packed)) exfat_bpb_t;

typedef struct {
  uint8_t   type;
  uint8_t   cnt;
  uint16_t  chksum;
  uint16_t  attr;
  uint8_t   custom[14];
  uint32_t  clu;
  uint64_t  size;
} __attribute__((packed)) exfat_dir_t;
typedef struct {
  uint8_t   type;
  uint8_t   flags;
  uint16_t  name[15];
} __attribute__((packed)) exfat_dir_n_t;
typedef struct {
  uint8_t   type;
  uint8_t   cnt;
  uint16_t  chksum;
  uint16_t  attr;
  uint16_t  res0;
  uint32_t  ct;
  uint32_t  mt;
  uint32_t  at;
  uint8_t   ctt;
  uint8_t   mtt;
  uint8_t   att;
  uint8_t   res1[9];
} __attribute__((packed)) exfat_dir_f_t;

uint64_t fat_size;
uint32_t spc, rc, data_lba, fat_cache, fat_lba, fat_clu, last, *fat;
uint16_t fat_attr, rsz, lfn[272], wcname[PATH_MAX];
uint8_t data[512];

/**
 * Get the next cluster in chain
 */
static uint32_t nextclu(uint32_t clu)
{
    uint64_t i;

    if(clu < 2 || clu >= 0x0FFFFFF7) return 0;
    if(clu < fat_cache || clu > fat_cache + 1023) {
        fat_cache = clu & ~1023;
        for(i = 0; i < 8; i++) loadsec(fat_lba + (fat_cache >> 7) + i, &fat[i << 7]);
    }
    clu = fat[clu - fat_cache];
    return clu < 2 || clu >= 0x0FFFFFF7 ? 0 : clu;
}

/**
 * Look up directories and return starting cluster and set file_size
 */
static int fat_open(char *fn)
{
    uint32_t lba, dc, ds, clu = rc;
    int i, n = 0;
    uint8_t *dir = data + sizeof(data);
    uint16_t secleft = 0, *u = lfn, *s = wcname, a, b, c, *d, attr;
    char *p = fn;

    if(!fn || !*fn) return 0;

    /* UTF-8 to lowercase WCHAR */
    for(p = fn, d = wcname; *p && d < &wcname[PATH_MAX - 2]; d++) {
        c = *p;
        if((*p & 128) != 0) {
            if(!(*p & 32)) { c = ((*p & 0x1F)<<6)|(*(p+1) & 0x3F); p += 1; } else
            if(!(*p & 16)) { c = ((*p & 0xF)<<12)|((*(p+1) & 0x3F)<<6)|(*(p+2) & 0x3F); p += 2; } else
            if(!(*p & 8)) { c = ((*p & 0x7)<<18)|((*(p+1) & 0x3F)<<12)|((*(p+2) & 0x3F)<<6)|(*(p+3) & 0x3F); p += 3; }
            else break;
        }
        p++;
        *d = c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c;
    }
    *d = 0;

    file_size = 0; fat_clu = clu = 0; fat_attr = 0;
    memset(lfn, 0, sizeof(lfn));
    while(1) {
        /* have we reached the end of the sector? */
        if(dir >= data + sizeof(data)) {
            if(secleft) { secleft--; lba++; }
            else {
                if(clu < 2 || clu >= 0x0FFFFFF7) return 0;
                secleft = spc - 1;
                lba = clu * spc + data_lba;
                clu = nextclu(clu);
            }
            loadsec(lba, &data);
            dir = data;
        }
        switch(dir[0]) {
            /* end of directory */
            case 0: return 0;
            /* primary directory entry */
            case 0x85:
                memset(lfn, 0, sizeof(lfn));
                u = lfn;
                n = ((exfat_dir_t*)dir)->cnt;
                attr = ((exfat_dir_t*)dir)->attr;
                dc = ds = 0;
            break;
            /* secondary entry, stream extension */
            case 0xc0:
                dc = ((exfat_dir_t*)dir)->clu;
                ds = ((exfat_dir_t*)dir)->size;
                n--;
            break;
            /* secondary entry, file name */
            case 0xc1:
                if(n) {
                    for(i = 0; i < 15; i++, u++) {
                        *u = dir[i*2+3] << 8 | dir[i*2+2];
                        if(!*u) break;
                    }
                    n--;
                }
                *u = 0;
            break;
        }
        if(!n && lfn[0]) {
            /* filename match? */
            if(*s == '/') s++;
            for(i = 0; lfn[i] && s[i] && s[i] != '/'; i++) {
                a = lfn[i]; if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
                b = s[i]; if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
                if(a != b) break;
            }
            if(!lfn[i]) {
                if(dc < 2 || dc >= 0x0FFFFFF7) return 0;
                /* is this a directory? */
                if(attr & 0x10) {
                    if(s[i] != '/') return 0;
                    /* go to subdirectory */
                    s += i + 1; n = secleft = 0; dir = data + sizeof(data);
                    clu = dc;
                    continue;
                } else {
                    /* no, it's a file, then we have located what we were looking for */
                    if(s[i]) return 0;
                    fat_clu = dc;
                    file_size = ds;
                    break;
                }
            }
        }
        dir += 32;
    }
    return 1;
}

/**
 * Read from opened file
 */
static uint64_t fat_read(uint64_t offs, uint64_t size, void *buf)
{
    uint32_t lba = 0, rem, o;
    uint16_t clu = fat_clu, nc, ns = 0, os = 0, rs = 512, secleft = 0;

    if(fat_clu < 2 || offs >= file_size || !size || !buf) return 0;
    if(offs + size > file_size) size = file_size - offs;
    rem = size;

    pb_init(size);
    if(offs) {
        nc = offs / (spc << 9); o = offs % (spc << 9);
        ns = o >> 9; os = o & 0x1ff; rs = 512 - os;
        if(nc) { while(nc-- && clu) { clu = nextclu(clu); } if(!clu) return 0; }
        secleft = spc - ns - 1;
        lba = clu * spc + ns - 1 + data_lba;
    }
    while(rem) {
        if(secleft) { secleft--; lba++; }
        else {
            if(!clu) break;
            secleft = spc - 1;
            lba = clu * spc + data_lba;
            clu = nextclu(clu);
        }
        if(rs > rem) rs = rem;
        if(rs < 512) {
            loadsec(lba, data);
            memcpy(buf, data + os, rs); os = 0;
        } else {
            loadsec(lba, buf);
            if(os) { memcpy(buf, buf + os, rs); os = 0; }
        }
        buf += rs; rem -= rs; rs = 512;
        pb_draw(size - rem);
    }
    pb_fini();
    return (uint64_t)(size - rem);
}

/**
 * Close the opened file
 */
static void fat_close(void)
{
    fat_clu = 0;
}

/**
 * Plugin entry point, check superblock and call sethooks
 */
PLG_API void _start(void)
{
    exfat_bpb_t *bpb = (exfat_bpb_t*)root_buf;
    uint32_t i;
    uint8_t *buf = root_buf;

    if(bpb->bps != 9 || !bpb->spc || !bpb->spf || !bpb->nf) return;
    /* save values from the "superblock" */
    spc = 1 << bpb->spc;
    rc = bpb->rc;
    fat_lba = bpb->fo + (bpb->flags & 1) * bpb->spf;
    data_lba = bpb->co;
    /* load portion of the FAT (overwrites bpb in memory) */
    for(i = 0; i < 8; i++, buf += 512)
        loadsec(fat_lba + i, buf);
    fat = (uint32_t*)root_buf;
    fat_cache = 0;

    sethooks(fat_open, fat_read, fat_close);
}
