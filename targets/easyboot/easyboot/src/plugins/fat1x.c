/*
 * src/plugins/fat1x.c
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
 * @brief Implements the FAT12/16 filesystems
 * https://social.technet.microsoft.com/wiki/contents/articles/6771.the-fat-file-system.aspx
 */

#include "../loader.h"

EASY_PLUGIN(PLG_T_FS) {
    { 54, 4, PLG_M_CONST, { 'F', 'A', 'T', '1' } }
};

uint32_t spc, root_lba, data_lba;
uint16_t *fat, last, fat_clu, rsz, lfn[272], wcname[PATH_MAX];
uint8_t data[512];

/**
 * Get the next cluster in chain
 */
static uint16_t fat_nextclu(uint16_t clu)
{
    uint16_t o;
    if(clu < 2 || clu >= last) return 0;
    if(last == 0xFF8) {
        o = fat[clu + (clu >> 1)];              /* FAT12 */
        clu = clu & 1 ? o >> 4 : o & 0xfff;
    } else clu = fat[clu];                      /* FAT16 */
    return clu < 2 || clu >= last ? 0 : clu;
}

/**
 * Look up directories and return starting cluster and set file_size
 */
static int fat_open(char *fn)
{
    uint32_t lba = root_lba;
    int i, n = 0, m = 0;
    uint8_t *dir = data + sizeof(data);
    uint16_t clu, secleft = rsz, *u, *s = wcname, a, c, *d;
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

    file_size = 0; fat_clu = clu = 0;
    memset(lfn, 0, sizeof(lfn));
    while(1) {
        /* have we reached the end of the sector? */
        if(dir >= data + sizeof(data)) {
            if(secleft) { secleft--; lba++; }
            else {
                if(clu < 2 || clu >= last) return 0;
                secleft = spc - 1;
                lba = clu * spc + data_lba;
                clu = fat_nextclu(clu);
            }
            loadsec(lba, &data);
            dir = data;
        }
        /* empty space? End of directory then */
        if(!dir[0]) return 0;
        /* not a deleted entry or current and parent entries? */
        if(dir[0] != 5 && dir[0] != 0xE5 && (dir[0] != '.' || (dir[1] != '.' && dir[1] != ' '))) {
            /* is this an LFN block? */
            if(dir[0xB] == 0xF) {
                /* first LFN block? */
                if(!n || (dir[0] & 0x40)) {
                    memset(lfn, 0, sizeof(lfn));
                    n = dir[0] & 0x1F;
                    /* bad record, not sure what to do. Let's reset state and continue with next entry */
                    if(n < 1 || n > 20) { n = m = 0; dir += 32; continue; }
                    u = lfn + (n - 1) * 13;
                }
                /* get the next part of UCS-2 characters */
                for(i = 0; i < 5; i++)
                    u[i] = dir[i*2+2] << 8 | dir[i*2+1];
                for(i = 0; i < 6; i++)
                    u[i+5] = dir[i*2+0xF] << 8 | dir[i*2+0xE];
                u[11] = dir[0x1D] << 8 | dir[0x1C];
                u[12] = dir[0x1F] << 8 | dir[0x1E];
                u -= 13;
                n--;
                /* indicate that the next directory entry belongs to an LFN */
                m = (!n && u < lfn);
            } else
            if(!(dir[0xB] & 8)) {
                /* if we don't have an LFN already, generate it from the 8.3 name in this entry */
                if(!m) {
                    for(i = 0; i < 8; i++) lfn[i] = dir[i];
                    while(i && lfn[i - 1] == ' ') i--;
                    if(dir[8] != ' ') {
                        lfn[i++] = '.'; lfn[i++] = dir[8];
                        if(dir[9] != ' ') {
                            lfn[i++] = dir[9];
                            if(dir[10] != ' ') { lfn[i++] = dir[10]; }
                        }
                    }
                    lfn[i] = 0;
                } else m = 0;
                /* filename match? */
                if(*s == '/') s++;
                for(i = 0; lfn[i] && s[i] && s[i] != '/'; i++) {
                    a = lfn[i]; if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
                    if(a != s[i]) break;
                }
                if(!lfn[i]) {
                    clu = (dir[0x1B] << 8) | dir[0x1A];
                    if(clu < 2 || clu >= last) return 0;
                    /* is this a directory? */
                    if(dir[0xB] & 0x10) {
                        if(s[i] != '/') return 0;
                        /* go to subdirectory */
                        s += i + 1; n = m = secleft = 0; dir = data + sizeof(data);
                        continue;
                    } else {
                        /* no, it's a file, then we have located what we were looking for */
                        if(s[i]) return 0;
                        fat_clu = clu;
                        file_size = (dir[0x1F] << 24) | (dir[0x1E] << 16) | (dir[0x1D] << 8) | dir[0x1C];
                        break;
                    }
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
        if(nc) { while(nc-- && clu) { clu = fat_nextclu(clu); } if(!clu) return 0; }
        secleft = spc - ns - 1;
        lba = clu * spc + ns - 1 + data_lba;
    }
    while(rem) {
        if(secleft) { secleft--; lba++; }
        else {
            if(!clu) break;
            secleft = spc - 1;
            lba = clu * spc + data_lba;
            clu = fat_nextclu(clu);
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
    esp_bpb_t *bpb = (esp_bpb_t*)root_buf;
    uint32_t i, spf, rsc;
    uint8_t *buf = root_buf;
    
    /* is it FAT 12 or 16? */
    if((bpb->fst[4] != '2' &&  bpb->fst[4] != '6') || bpb->bps != 512 || !bpb->spc || !bpb->spf16 || bpb->spf16 > 256) return;
    /* save values from the "superblock" */
    spc = bpb->spc; last = bpb->fst[4] == '2' ? 0xFF8 : 0xFFF8;
    rsz = ((bpb->nr * sizeof(esp_dir_t) + 511) >> 9);
    root_lba = bpb->spf16 * bpb->nf + bpb->rsc;
    data_lba = root_lba + rsz;
    /* load the FAT (overwrites bpb in memory) */
    for(i = 0, spf = bpb->spf16, rsc = bpb->rsc; i < spf; i++, buf += 512)
        loadsec(rsc + i, buf);
    fat = (uint16_t*)root_buf;

    sethooks(fat_open, fat_read, fat_close);
}
