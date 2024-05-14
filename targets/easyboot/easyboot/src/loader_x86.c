/*
 * src/loader_x86.c
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
 * @brief The main Easyboot loader program on x86_64
 *
 * Memory layout when booted on UEFI:
 *     Arbitrary, uses relocations. Data buffers are allocated through BootServices
 *    0x510 -   0x550   AP startup data
 *  0x1F000 - 0x20000   relocated AP startup code
 *
 * Memory layout when booted on BIOS:
 *      0x0 -   0x400   IVT
 *    0x400 -   0x4FF   BDA
 *    0x4FF -   0x500   BIOS boot drive code
 *    0x500 -   0x510   BIOS LBA packet
 *    0x510 -   0x520   GDT value
 *    0x520 -   0x530   IDT value
 *    0x530 -   0x538   page table root
 *    0x538 -   0x540   kernel entry point (also SMP semaphor)
 *    0x540 -   0x548   tags_buf
 *    0x548 -   0x550   CPU clockcycles in 1 msec
 *    0x580 -   0x600   EDID info
 *    0x600 -   0x800   temporary disk, E820 or VESA buffer
 *    0x800 -   0xB00   temporary VESA buffer continued
 *    0xB00 -  0x1000   stack (ca. 1280 bytes)
 *   0x1000 -  0x8000   paging tables
 *   0x8000 - 0x20000   our COFF sections (0x1F000 - 0x20000 relocated AP startup code)
 *  0x20000 - 0x40000   config + tags
 *  0x40000 - 0x90000   plugin ids; from the top to bottom: kernel's stack
 *  0x90000 - 0x9A000   Linux kernel only: zero page + cmdline
 *  0x9A000 - 0xA0000   EBDA
 *  0xA0000 - 0xFFFFF   VRAM and BIOS ROM
 * 0x100000 -      x    kernel segments, followed by the modules, each page aligned
 *
 * Plugin ids:
 * byte 0..1: record length
 * byte 2..9: loaded plugin's address
 * byte 10: plugin type
 * byte 11: number of id matches (n)
 * byte 12..11 + n * 8: id matches (plg_id_t)
 * byte 12 + n * 8: zero terminated UTF-8 name of the plugin
 */

/**
 * Specify where the boot messages should appear, make it a comment to disable
 */
#define CONSOLE_SERIAL  0x3f8                       /* default serial, IO base address of COM1 port */
#define CONSOLE_FB                                  /* on screen too */
/*#define CONSOLE_VGA*/                             /* fallback text mode console in BIOS */
/*#define CONSOLE_BOCHS_E9*/                        /* bochs E9 port hack */
/*#define CONSOLE_UEFI*/                            /* UEFI ConOut */

/* it is VERY important that these two variables must be the first in the
 * read-only data segment, because the disk generator might alter them */
const char defkernel[64] = "kernel", definitrd[64] = "";
const char copy[] = "Easyboot loader, Copyright (c) 2023 bzt, GPLv3+\n";

#include "loader.h"
#include "nyx_api.h"
#include "paging_uefi_x64.h"
kAFL_payload *payload_buffer;
int fuzz_start= 0;
/* IMPORTANT: don't assume .bss is zeroed out like in a hosted environment, because it's not */
uint64_t file_size, file_base, file_buf, ram, *pt, hack_buf, krnl_size;
uint32_t fb_fg, fb_bg, pb_bg, pb_b, pb_m, pb_l, verbose, num_memmap, edid_size;
uint8_t cons, *tags_buf, *tags_ptr, *rsdp_ptr, *dsdt_ptr, *edid_ptr, *plgids, *root_buf, *krnl_buf, *pb_fb, in_exc, rddir, smp;
uint64_t data_lba, fat_lba, root_lba, root_siz;
uint8_t vbr[512], data[512];
uint32_t fat[1024], fat_cache, file_clu;
uint16_t lfn[272], wcname[PATH_MAX];
guid_t rootuuid;
esp_bpb_t *bpb;
multiboot_mmap_entry_t *memmap;
multiboot_tag_module_t *initrd;
multiboot_tag_framebuffer_t vidmode;
efi_system_table_t *ST;
efi_handle_t *IM;
efi_boot_services_t *BS;
efi_block_io_t *bio;
efi_file_handle_t *root_dir;
efi_file_handle_t *f;
efi_file_info_t info, dirent;
efi_device_path_t *bootdev;
char fn[PATH_MAX], *conf_buf, *conf_ptr, *kernel, *cmdline;
/* first one is common, others are architecture dependent */
uint64_t plg_imsk[] = { PLG_IM_0 };
plg_got_t plg_got = {
    &verbose, &file_size, &root_buf, &tags_buf, &tags_ptr, &rsdp_ptr, &dsdt_ptr, &ST,
    &memset, &memcpy, &memcmp, &alloc, &free,
    &printf, &pb_init, &pb_draw, &pb_fini,
    &loadsec, &sethooks,
    &fw_open, &fw_read, &fw_close, &fw_loadfile,
    &fw_loadseg
};

const char excstr[32][3] = { "DE", "DB", "NI", "BP", "OF", "BR", "UD", "DF", "CO", "TS", "NP", "SS", "GP", "PF",
    "15", "MF", "AC", "MC", "XF", "20", "CP", "22", "23", "24", "25", "26", "27", "HV", "VC", "SX", "31" };

typedef struct {
    char magic[4];
    uint32_t size;
    uint8_t rev;
    uint8_t chksum;
    uint8_t res0[30];
    uint32_t dsdt;
    uint8_t  reserved[96];
    uint64_t x_dsdt;
} __attribute__((packed)) fadt_t;

/**************** Mandatory functions, Clang generates calls to them ****************/

PLG_API void memcpy(void *dst, const void *src, uint32_t n) { __asm__ __volatile__("repnz movsb"::"D"(dst),"S"(src),"c"(n):); }
PLG_API void memset(void *dst, uint8_t c, uint32_t n) { __asm__ __volatile__("repnz stosb"::"D"(dst),"a"(c),"c"(n):); }
PLG_API int  memcmp(const void *s1, const void *s2, uint32_t n) {
    int ret;
    __asm__ __volatile__("cld;repe cmpsb;xorl %%eax,%%eax;movb -1(%%rdi), %%al;subb -1(%%rsi), %%al;"
    :"=a"(ret)
    :"D"(s1),"S"(s2),"c"(n):);
    return ret;
}

/**
 * Convert hex string into integer
 */
char *gethex(char *ptr, uint32_t *ret, int len)
{
    *ret = 0;
    for(;len--;ptr++) {
        if(*ptr>='0' && *ptr<='9') {          *ret <<= 4; *ret += (uint32_t)(*ptr - '0'); }
        else if(*ptr >= 'a' && *ptr <= 'f') { *ret <<= 4; *ret += (uint32_t)(*ptr - 'a' + 10); }
        else if(*ptr >= 'A' && *ptr <= 'F') { *ret <<= 4; *ret += (uint32_t)(*ptr - 'A' + 10); }
        else break;
    }
    return ptr;
}

/**
 * Convert decimal string to integer
 */
char *getint(char *ptr, uint32_t *ret)
{
    for(*ret = 0; *ptr >= '0' && *ptr <= '9'; ptr++) { *ret *= 10; *ret += (uint32_t)(*ptr - '0'); }
    return ptr;
}

/**
 * Parse a GUID in string into binary representation
 */
char *getguid(char *ptr, guid_t *guid)
{
    uint32_t v;
    int i;

    if(!ptr || !*ptr || ptr[8] != '-' || ptr[13] != '-' || ptr[18] != '-') return ptr;
    memset(guid, 0, sizeof(guid_t));
    ptr = gethex(ptr, &v, 8); guid->Data1 = v; ptr++;
    ptr = gethex(ptr, &v, 4); guid->Data2 = v; ptr++;
    ptr = gethex(ptr, &v, 4); guid->Data3 = v; ptr++;
    ptr = gethex(ptr, &v, 2); guid->Data4[0] = v;
    ptr = gethex(ptr, &v, 2); guid->Data4[1] = v; if(*ptr == '-') ptr++;
    for(i = 2; i < 8; i++) { ptr = gethex(ptr, &v, 2); guid->Data4[i] = v; }
    return ptr;
}

/**
 * Convert UTF-8 to UNICODE
 */
uint32_t utf8(char **s)
{
    uint32_t c = **s;

    if((**s & 128) != 0) {
        if(!(**s & 32)) { c = ((**s & 0x1F)<<6)|(*(*s+1) & 0x3F); *s += 1; } else
        if(!(**s & 16)) { c = ((**s & 0xF)<<12)|((*(*s+1) & 0x3F)<<6)|(*(*s+2) & 0x3F); *s += 2; } else
        if(!(**s & 8)) { c = ((**s & 0x7)<<18)|((*(*s+1) & 0x3F)<<12)|((*(*s+2) & 0x3F)<<6)|(*(*s+3) & 0x3F); *s += 3; }
        else c = 0;
    }
    (*s)++;
    return c;
}

/**
 * Convert UCS2 string to UTF-8
 */
uint8_t *ucs2(uint8_t *s, uint16_t *u)
{
    uint8_t *e = s + 31;
    for(; *u && s < e; u++) {
        if(*u < 0x80) { *s++ = *u; } else
        if(*u < 0x800) { *s++ = ((*u>>6)&0x1F)|0xC0; *s++ = (*u&0x3F)|0x80; } else
        { *s++ = ((*u>>12)&0x0F)|0xE0; *s++ = ((*u>>6)&0x3F)|0x80; *s++ = (*u&0x3F)|0x80; }
    }
    *s++ = 0;
    return s;
}

/**************** Early boot console ****************/

#ifdef CONSOLE_FB
uint8_t key_tga[645] = { 0,1,9,0,0,35,0,24,0,0,0,0,24,0,23,0,8,32,0,0,0,92,95,100,95,96,98,101,99,103,100,100,106,103,104,106,107,108,113,111,113,118,116,116,121,125,125,131,131,133,139,139,140,147,142,144,148,146,150,153,152,156,162,158,163,168,161,166,170,169,173,178,175,178,184,179,183,189,185,190,194,189,196,199,196,200,204,200,205,208,205,210,213,212,217,220,217,222,225,221,226,229,224,227,228,224,228,231,226,231,234,227,232,235,232,238,242,234,240,243,0,0,0,6,0,28,25,24,22,21,19,139,18,4,19,21,23,28,0,11,28,27,29,26,25,24,22,21,20,19,18,18,133,19,5,20,22,24,25,29,23,3,25,26,27,30,131,27,1,25,25,130,24,0,23,130,22,6,21,21,24,27,30,30,23,23,23,25,25,29,33,32,31,30,27,27,26,25,25,24,24,23,22,22,21,21,31,29,27,26,23,22,23,24,27,32,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,30,27,26,25,20,19,21,22,26,32,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,29,130,25,23,17,19,20,25,33,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,27,25,24,24,23,16,17,18,24,33,32,31,30,27,27,26,25,25,24,24,23,22,22,21,21,27,25,24,23,23,14,16,17,24,33,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,26,24,23,22,23,13,14,15,23,33,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,25,23,22,22,23,13,13,14,22,32,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,25,22,21,22,130,13,20,22,32,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,25,21,21,22,130,13,20,22,32,32,31,30,30,27,26,25,25,24,24,23,22,21,21,20,25,22,21,22,20,13,14,13,22,32,32,31,30,27,27,26,25,25,24,24,22,22,21,21,20,25,130,22,23,13,13,11,21,32,32,31,30,27,27,26,25,25,24,24,22,22,21,21,20,23,20,22,22,23,12,11,10,21,32,32,31,30,27,26,26,25,25,24,24,22,22,21,21,20,22,17,20,22,16,10,10,9,17,32,32,31,30,27,26,26,25,25,24,24,22,22,130,21,3,19,14,17,19,23,9,9,7,6,25,33,32,31,31,30,30,27,27,26,25,24,24,23,24,23,9,11,14,17,1,8,7,130,4,2,10,11,12,134,13,8,12,11,10,9,4,6,9,11,14,0,7,130,4,3,6,7,8,9,131,10,131,9,7,8,7,6,4,4,6,9,10,11,3,4,4,6,7,8,9,9,10,9,10,10,132,9,6,8,7,6,4,4,6,3,4,2,4,6,7,8,140,9,5,8,8,6,4,1,2,1,0,2,147,5,1,2,0 };
uint8_t font_sfn[2174] = { 83,70,78,50,126,8,0,0,0,0,8,16,11,13,38,0,15,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,1,24,24,128,0,126,128,9,91,64,1,65,64,1,65,64,1,109,128,6,24,60,60,60,24,24,24,128,3,102,102,102,36,128,8,54,54,127,54,54,54,127,54,54,128,13,24,24,62,99,67,3,62,96,96,97,99,62,24,24,128,7,67,99,48,24,12,6,99,97,128,9,28,54,54,28,110,59,51,51,51,110,128,3,12,12,12,4,128,9,48,24,12,12,12,12,12,12,24,48,128,9,12,24,48,48,48,48,48,48,24,12,128,4,102,60,255,60,102,128,4,24,24,126,24,24,128,3,24,24,24,12,128,0,127,128,7,64,96,48,24,12,6,3,1,128,9,28,54,99,99,107,107,99,99,54,28,128,9,24,28,30,24,24,24,24,24,24,126,128,9,62,99,96,48,24,12,6,3,99,127,128,9,62,99,96,96,60,96,96,96,99,62,128,9,48,56,60,54,51,127,48,48,48,120,128,9,127,3,3,3,63,96,96,96,99,62,128,9,28,6,3,3,63,99,99,99,99,62,128,9,127,99,96,96,48,24,12,12,12,12,128,9,62,99,99,99,62,99,99,99,99,62,128,9,62,99,99,99,126,96,96,96,48,30,128,2,24,24,12,128,8,96,48,24,12,6,12,24,48,96,128,8,6,12,24,48,96,48,24,12,6,128,6,62,99,99,48,24,24,24,128,8,62,99,99,123,123,123,59,3,62,128,9,8,28,54,99,99,127,99,99,99,99,128,9,63,102,102,102,62,102,102,102,102,63,128,9,60,102,67,3,3,3,3,67,102,60,128,9,31,54,102,102,102,102,102,102,54,31,128,9,127,102,70,22,30,22,6,70,102,127,128,9,127,102,70,22,30,22,6,6,6,15,128,9,60,102,67,3,3,123,99,99,102,92,128,9,99,99,99,99,127,99,99,99,99,99,128,9,60,24,24,24,24,24,24,24,24,60,128,9,120,48,48,48,48,48,51,51,51,30,128,9,103,102,102,54,30,30,54,102,102,103,128,9,15,6,6,6,6,6,6,70,102,127,128,9,99,119,127,127,107,99,99,99,99,99,128,9,99,103,111,127,123,115,99,99,99,99,128,9,62,99,99,99,99,99,99,99,99,62,128,9,63,102,102,102,62,6,6,6,6,15,128,11,62,99,99,99,99,99,99,107,123,62,48,112,128,9,63,102,102,102,62,54,102,102,102,103,128,9,62,99,99,6,28,48,96,99,99,62,128,9,126,126,90,24,24,24,24,24,24,60,128,9,99,99,99,99,99,99,99,99,99,62,128,9,99,99,99,99,99,99,99,54,28,8,128,9,99,99,99,99,107,107,107,127,119,54,128,9,99,99,54,62,28,28,62,54,99,99,128,9,102,102,102,102,60,24,24,24,24,60,128,9,127,99,97,48,24,12,6,67,99,127,128,9,60,12,12,12,12,12,12,12,12,60,128,8,1,3,7,14,28,56,112,96,64,128,9,60,48,48,48,48,48,48,48,48,60,128,3,8,28,54,99,128,0,255,128,2,12,12,24,128,6,30,48,62,51,51,51,110,128,9,7,6,6,30,54,102,102,102,102,62,128,6,62,99,3,3,3,99,62,128,9,56,48,48,60,54,51,51,51,51,110,128,6,62,99,127,3,3,99,62,128,9,28,54,38,6,15,6,6,6,6,15,128,9,110,51,51,51,51,51,62,48,51,30,128,9,7,6,6,54,110,102,102,102,102,103,128,6,28,24,24,24,24,24,60,128,1,96,96,128,9,112,96,96,96,96,96,96,102,102,60,128,9,7,6,6,102,54,30,30,54,102,103,128,9,28,24,24,24,24,24,24,24,24,60,128,6,55,127,107,107,107,107,99,128,6,59,102,102,102,102,102,102,128,6,62,99,99,99,99,99,62,128,9,59,102,102,102,102,102,62,6,6,15,128,9,110,51,51,51,51,51,62,48,48,120,128,6,59,110,102,6,6,6,15,128,6,62,99,6,28,48,99,62,128,9,8,12,12,63,12,12,12,12,108,56,128,6,51,51,51,51,51,51,110,128,6,102,102,102,102,102,60,24,128,6,99,99,107,107,107,127,54,128,6,99,54,28,28,28,54,99,128,9,99,99,99,99,99,99,126,96,48,31,128,6,127,51,24,12,6,99,127,128,9,112,24,24,24,14,24,24,24,24,112,128,11,24,24,24,24,24,24,24,24,24,24,24,24,128,9,14,24,24,24,112,24,24,24,24,14,128,1,110,59,128,6,8,28,54,99,99,99,127,0,1,8,16,9,0,0,2,45,0,0,158,0,0,8,16,9,0,0,2,8,16,9,0,0,2,57,0,0,0,10,38,0,0,0,1,8,16,9,0,0,1,66,0,0,0,1,8,16,9,0,0,3,72,0,0,0,1,8,16,9,0,0,0,83,0,0,0,1,8,16,9,0,0,4,99,0,0,0,1,8,16,9,0,0,2,109,0,0,0,1,8,16,9,0,0,1,121,0,0,0,1,8,16,9,0,0,2,127,0,0,0,1,8,16,9,0,0,2,139,0,0,0,1,8,16,9,0,0,5,151,0,0,0,1,8,16,9,0,0,5,158,0,0,0,1,8,16,9,0,0,9,165,0,0,0,1,8,16,9,0,0,7,171,0,0,0,1,8,16,9,0,0,10,38,0,0,0,1,8,16,9,0,0,4,174,0,0,0,1,8,16,9,0,0,2,184,0,0,0,1,8,16,9,0,0,2,196,0,0,0,1,8,16,9,0,0,2,208,0,0,0,1,8,16,9,0,0,2,220,0,0,0,1,8,16,9,0,0,2,232,0,0,0,1,8,16,9,0,0,2,244,0,0,0,1,8,16,9,0,0,2,0,1,0,0,1,8,16,9,0,0,2,12,1,0,0,1,8,16,9,0,0,2,24,1,0,0,1,8,16,9,0,0,2,36,1,0,0,2,8,16,9,0,0,4,38,0,0,0,9,38,0,0,0,2,8,16,9,0,0,4,38,0,0,0,9,48,1,0,0,1,8,16,9,0,0,3,53,1,0,0,2,8,16,9,0,0,5,42,0,0,0,8,42,0,0,0,1,8,16,9,0,0,3,64,1,0,0,2,8,16,9,0,0,2,75,1,0,0,10,38,0,0,0,1,8,16,9,0,0,3,84,1,0,0,1,8,16,9,0,0,2,95,1,0,0,1,8,16,9,0,0,2,107,1,0,0,1,8,16,9,0,0,2,119,1,0,0,1,8,16,9,0,0,2,131,1,0,0,1,8,16,9,0,0,2,143,1,0,0,1,8,16,9,0,0,2,155,1,0,0,1,8,16,9,0,0,2,167,1,0,0,1,8,16,9,0,0,2,179,1,0,0,1,8,16,9,0,0,2,191,1,0,0,1,8,16,9,0,0,2,203,1,0,0,1,8,16,9,0,0,2,215,1,0,0,1,8,16,9,0,0,2,227,1,0,0,1,8,16,9,0,0,2,239,1,0,0,1,8,16,9,0,0,2,251,1,0,0,1,8,16,9,0,0,2,7,2,0,0,1,8,16,9,0,0,2,19,2,0,0,1,8,16,9,0,0,2,31,2,0,0,1,8,16,9,0,0,2,45,2,0,0,1,8,16,9,0,0,2,57,2,0,0,1,8,16,9,0,0,2,69,2,0,0,1,8,16,9,0,0,2,81,2,0,0,1,8,16,9,0,0,2,93,2,0,0,1,8,16,9,0,0,2,105,2,0,0,1,8,16,9,0,0,2,117,2,0,0,1,8,16,9,0,0,2,129,2,0,0,1,8,16,9,0,0,2,141,2,0,0,1,8,16,9,0,0,2,153,2,0,0,1,8,16,9,0,0,3,165,2,0,0,1,8,16,9,0,0,2,176,2,0,0,1,8,16,9,0,0,0,188,2,0,0,1,8,16,9,0,0,13,194,2,0,0,1,8,16,9,0,0,0,197,2,0,0,1,8,16,9,0,0,5,202,2,0,0,1,8,16,9,0,0,2,211,2,0,0,1,8,16,9,0,0,5,223,2,0,0,1,8,16,9,0,0,2,232,2,0,0,1,8,16,9,0,0,5,244,2,0,0,1,8,16,9,0,0,2,253,2,0,0,1,8,16,9,0,0,5,9,3,0,0,1,8,16,9,0,0,2,21,3,0,0,2,8,16,9,0,0,2,38,0,0,0,5,33,3,0,0,2,8,16,9,0,0,2,42,3,0,0,5,46,3,0,0,1,8,16,9,0,0,2,58,3,0,0,1,8,16,9,0,0,2,70,3,0,0,1,8,16,9,0,0,5,82,3,0,0,1,8,16,9,0,0,5,91,3,0,0,1,8,16,9,0,0,5,100,3,0,0,1,8,16,9,0,0,5,109,3,0,0,1,8,16,9,0,0,5,121,3,0,0,1,8,16,9,0,0,5,133,3,0,0,1,8,16,9,0,0,5,142,3,0,0,1,8,16,9,0,0,2,151,3,0,0,1,8,16,9,0,0,5,163,3,0,0,1,8,16,9,0,0,5,172,3,0,0,1,8,16,9,0,0,5,181,3,0,0,1,8,16,9,0,0,5,190,3,0,0,1,8,16,9,0,0,5,199,3,0,0,1,8,16,9,0,0,5,211,3,0,0,1,8,16,9,0,0,2,220,3,0,0,1,8,16,9,0,0,2,232,3,0,0,1,8,16,9,0,0,2,246,3,0,0,1,8,16,9,0,0,2,2,4,0,0,1,8,16,9,0,0,4,6,4,0,159,0,0,0,0,9,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,254,255,254,255,254,255,254,255,195,95,50,78,70,83 };
typedef struct { uint8_t magic[4]; uint32_t size; uint8_t type; uint8_t features; uint8_t width; uint8_t height; uint8_t baseline; uint8_t underline; uint16_t fragments_offs; uint32_t characters_offs; uint32_t ligature_offs; uint32_t kerning_offs; uint32_t cmap_offs; } __attribute__((packed)) ssfn_t;
ssfn_t *font = (ssfn_t*)font_sfn;
uint32_t fb_x, fb_y;
#endif
#ifdef CONSOLE_VGA
uint16_t vga_x, vga_y;
#endif

/**
 * Initialize the console
 */
void console_init(void)
{
#ifdef CONSOLE_SERIAL
    __asm__ __volatile__(
        "movl %0, %%edx;"
        "xorb %%al, %%al;outb %%al, %%dx;"               /* IER int off */
        "movb $0x80, %%al;addb $2,%%dl;outb %%al, %%dx;" /* LCR set divisor mode */
        "movb $1, %%al;subb $3,%%dl;outb %%al, %%dx;"    /* DLL divisor lo 115200 */
        "xorb %%al, %%al;incb %%dl;outb %%al, %%dx;"     /* DLH divisor hi */
        "incb %%dl;outb %%al, %%dx;"                     /* FCR fifo off */
        "movb $0x43, %%al;incb %%dl;outb %%al, %%dx;"    /* LCR 8N1, break on */
        "movb $0x8, %%al;incb %%dl;outb %%al, %%dx;"     /* MCR Aux out 2 */
        "xorb %%al, %%al;subb $4,%%dl;inb %%dx, %%al"    /* clear receiver/transmitter */
    : : "a"(CONSOLE_SERIAL + 1): "rdx");
#endif
#ifdef CONSOLE_FB
    fb_x = fb_y = 4;
#endif
#ifdef CONSOLE_VGA
    vga_x = vga_y = 0;
    if(!vidmode.framebuffer_addr && !ST) memset((void*)0xB8000, 0, 160 * 25);
#endif
#ifdef CONSOLE_UEFI
    if(ST && ST->ConOut) ST->ConOut->Reset(ST->ConOut, 0);
#endif
    cons = 3;
}

/**
 * Display a character on boot console
 */
void console_putc(uint32_t c)
{
#ifdef CONSOLE_SERIAL
    int i;
    uint8_t str[4];
#endif
#if defined(CONSOLE_FB) | defined(CONSOLE_VGA)
    uint8_t *fb = (uint8_t*)vidmode.framebuffer_addr;
#endif
#ifdef CONSOLE_UEFI
    uint16_t tmp[2];
#endif
#ifdef CONSOLE_FB
    uintptr_t o, p;
    uint32_t j, k, l, m, n, bpp = ((vidmode.framebuffer_bpp + 7) >> 3), C;
    uint8_t *ptr, *chr, *frg;

    if(fb && (cons & 2))
        switch(c) {
            case '\r': fb_x = 4; break;
            case '\n': fb_x = 4; fb_y += font->height; break;
            default:
                if(fb_x + font->width + 5 >= vidmode.framebuffer_width) { fb_x = 4; fb_y += font->height; }
                if(fb_y + font->height + 5 > vidmode.framebuffer_height) {
                    j = fb_y; fb_y = vidmode.framebuffer_height - font->height - 5; j -= fb_y;
                    m = 0; l = j * vidmode.framebuffer_pitch;
                    for(k = j; k < vidmode.framebuffer_height; k++,
                      m += vidmode.framebuffer_pitch, l += vidmode.framebuffer_pitch)
                        memcpy(fb + m, fb + l, vidmode.framebuffer_pitch);
                    for(k = fb_y, m = fb_y * vidmode.framebuffer_pitch; k < vidmode.framebuffer_height; k++,
                      m += vidmode.framebuffer_pitch)
                        memset(fb + m, 0, vidmode.framebuffer_pitch);
                }
                for(ptr = (uint8_t*)font + font->characters_offs, chr = 0, i = 0; i < 0x110000; i++) {
                    if(ptr[0] == 0xFF) { i += 65535; ptr++; }
                    else if((ptr[0] & 0xC0) == 0xC0) { j = (((ptr[0] & 0x3F) << 8) | ptr[1]); i += j; ptr += 2; }
                    else if((ptr[0] & 0xC0) == 0x80) { j = (ptr[0] & 0x3F); i += j; ptr++; }
                    else { if((unsigned int)i == c) { chr = ptr; break; } ptr += 6 + ptr[1] * (ptr[0] & 0x40 ? 6 : 5); }
                }
                if(!chr) chr = (uint8_t*)font + font->characters_offs;
                ptr = chr + 6; o = (uintptr_t)fb + fb_y * vidmode.framebuffer_pitch + fb_x * bpp;
                for(i = n = 0; i < chr[1]; i++, ptr += chr[0] & 0x40 ? 6 : 5) {
                    if(ptr[0] == 255 && ptr[1] == 255) continue;
                    frg = (uint8_t*)font + (chr[0] & 0x40 ? ((ptr[5] << 24) | (ptr[4] << 16) | (ptr[3] << 8) | ptr[2]) :
                        ((ptr[4] << 16) | (ptr[3] << 8) | ptr[2]));
                    if((frg[0] & 0xE0) != 0x80) continue;
                    o += (int)(ptr[1] - n) * vidmode.framebuffer_pitch; n = ptr[1];
                    k = ((frg[0] & 0x1F) + 1) << 3; j = frg[1] + 1; frg += 2;
                    for(m = 1; j; j--, n++, o += vidmode.framebuffer_pitch)
                        for(p = o, l = 0; l < k; l++, p += bpp, m <<= 1) {
                            if(m > 0x80) { frg++; m = 1; }
                            if((*frg & m) || !(cons & 8)) {
                                C = *frg & m ? fb_fg : fb_bg;
                                switch(vidmode.framebuffer_bpp) {
                                    case 15: case 16: *((uint16_t*)p) = C; break;
                                    case 24: case 32: *((uint32_t*)p) = C; break;
                                }
                            }
                        }
                }
                fb_x += chr[4]; fb_y += chr[5];
            break;
        }
#endif
    if(cons & 1) {
#ifdef CONSOLE_SERIAL
        if(c<0x80) { str[0] = c; str[1] = 0; } else
        if(c<0x800) { str[0]=((c>>6)&0x1F)|0xC0; str[1]=(c&0x3F)|0x80; str[2] = 0; }
        else { str[0]=((c>>12)&0x0F)|0xE0; str[1]=((c>>6)&0x3F)|0x80; str[2]=(c&0x3F)|0x80; str[3] = 0; }
        if(str[0] == '\n') { str[0] = '\r'; str[1] = '\n'; }
        for(i = 0; str[i]; i++)
            __asm__ __volatile__(
                "xorl %%ebx, %%ebx; movb %0, %%bl;"
                "movl $10000,%%ecx;"
                "1:inb %%dx, %%al;pause;"
                "cmpb $0xff,%%al;je 2f;"
                "dec %%ecx;jz 2f;"
                "andb $0x20,%%al;jz 1b;"
                "subb $5,%%dl;movb %%bl, %%al;outb %%al, %%dx;2:;"
#ifdef CONSOLE_BOCHS_E9
                "movb %%bl, %%al;outb %%al, $0xe9;"
#endif
            ::"a"(str[i]),"d"(CONSOLE_SERIAL + 5): "rbx", "rcx");
#endif
#ifdef CONSOLE_UEFI
        if(ST && ST->ConOut) { tmp[0] = c; tmp[1] = 0; ST->ConOut->OutputString(ST->ConOut, tmp); }
#endif
#ifdef CONSOLE_VGA
        if(!fb && !ST)
            switch(c) {
                case '\r': vga_x = 0; break;
                case '\n': vga_x = 0; vga_y++; break;
                default:
                    if(vga_y >= 25) {
                        memcpy((void*)0xB8000, (void*)0xB8000 + 160, 160 * 24); vga_x = 0; vga_y = 24;
                        memset((void*)0xB8000 + 24 * 160, 0, 160);
                    }
                    *((uint16_t*)((uintptr_t)0xB8000 + vga_y * 160 + vga_x++ * 2)) = 0x0f00 | (c & 0xff);
                break;
            }
#endif
    }
    (void)c;
}

/**
 * Display (extremely minimal) formated message on console
 * %c: an ASCII character
 * %d: a decimal number
 * %x: a hexadecimal number
 * %p: a pointer
 * %s: a zero terminated ASCII string (8 bit)
 * %S: a zero terminated WCHAR string (16 bit characters, truncated to 8 bit)
 * %D: dump 16 bytes from given address
 */
PLG_API void printf(char *fmt, ...)
{
    __builtin_va_list args;
    uint8_t *ptr;
    int64_t arg;
    uint16_t *u;
    int len, sign, i, l;
    char *p, tmpstr[19], n;

    __builtin_va_start(args, fmt);
    arg = 0;
    while(*fmt) {
        if(*fmt == '%') {
            fmt++;
            if(*fmt == '%') goto put;
            len=l=0; while(*fmt >= '0' && *fmt <= '9') { len *= 10; len += *fmt - '0'; fmt++; }
            if(*fmt == 'l') { l++; fmt++; }
            if(*fmt == 'c') { arg = __builtin_va_arg(args, int); console_putc((uint8_t)arg); fmt++; continue; } else
            if(*fmt == 'd') {
                if(!l) arg = (int32_t)__builtin_va_arg(args, int32_t);
                else arg = __builtin_va_arg(args, int64_t);
                sign = 0; if((int)arg < 0) { arg = -arg; sign++; }
                i = 18; tmpstr[i] = 0;
                do { tmpstr[--i] = '0' + (arg % 10); arg /= 10; } while(arg != 0 && i > 0);
                if(sign) tmpstr[--i] = '-';
                if(len > 0 && len < 18) { while(i > 18 - len) tmpstr[--i] = ' '; }
                p = &tmpstr[i];
                goto putstring;
            } else
            if(*fmt == 'x' || *fmt == 'p') {
                if(*fmt == 'x' && !l) arg = (int32_t)__builtin_va_arg(args, int32_t);
                else arg = __builtin_va_arg(args, int64_t);
                i = 16; tmpstr[i] = 0; if(*fmt == 'p') len = 16;
                do { n = arg & 0xf; tmpstr[--i] = n + (n > 9 ? 0x37 : 0x30); arg >>= 4; } while(arg != 0 && i > 0);
                if(len > 0 && len <= 16) { while(i > 16 - len) tmpstr[--i] = '0'; }
                p = &tmpstr[i];
                goto putstring;
            } else
            if(*fmt == 's') {
                p = __builtin_va_arg(args, char*);
putstring:      if(p == (void*)0) p = "(null)";
                while(*p) console_putc(utf8(&p));
            }
            if(*fmt == 'S') {
                u = __builtin_va_arg(args, uint16_t*);
                if(u == (void*)0) u = L"(null)";
                while(*u) console_putc(*u++);
            } else
            if(*fmt == 'D') {
                arg = __builtin_va_arg(args, int64_t);
                if(len < 1) len = 1;
                do {
                    for(i = 28; i >= 0; i -= 4) { n = (arg >> i) & 15; n += (n>9?0x37:0x30); console_putc(n); }
                    console_putc(':'); console_putc(' ');
                    ptr = (uint8_t*)(uintptr_t)arg;
                    for(i = 0; i < 16; i++) {
                        n = (ptr[i] >> 4) & 15; n += (n>9?0x37:0x30); console_putc(n);
                        n = ptr[i] & 15; n += (n>9?0x37:0x30); console_putc(n);
                        console_putc(' ');
                    }
                    console_putc(' ');
                    for(i = 0; i < 16; i++)
                        console_putc(ptr[i] < 32 || ptr[i] >= 127 ? '.' : ptr[i]);
                    console_putc('\n');
                    arg += 16;
                } while(--len);
            }
            fmt++;
        } else {
put:        console_putc(utf8(&fmt));
        }
    }
    __builtin_va_end(args);
}

/**************** Progress bar ****************/

void pb_dot(uint32_t i, uint32_t c)
{
    switch(vidmode.framebuffer_bpp) {
        case 15: case 16: *((uint16_t*)(pb_fb + i)) = *((uint16_t*)(pb_fb + vidmode.framebuffer_pitch + i)) = c; break;
        case 24: case 32: *((uint32_t*)(pb_fb + i)) = *((uint32_t*)(pb_fb + vidmode.framebuffer_pitch + i)) = c; break;
    }
}

/**
 * Initialize the progress bar
 */
PLG_API uint64_t pb_init(uint64_t size)
{
    uint32_t i, x;

    pb_fb = NULL; pb_m = (size >> 9) + 1; pb_l = 0;
    if(!vidmode.framebuffer_addr || !size || pb_m < vidmode.framebuffer_width - 4) return 0;
    pb_b = (vidmode.framebuffer_bpp + 7) >> 3;
    pb_fb = (uint8_t*)vidmode.framebuffer_addr + (vidmode.framebuffer_height - 4) * vidmode.framebuffer_pitch + 2 * pb_b;
    for(i = x = 0; x < vidmode.framebuffer_width - 4; x++, i += pb_b)
        pb_dot(i, pb_bg);
    return size / (vidmode.framebuffer_width - 4);
}

/**
 * Draw the progress bar
 */
PLG_API void pb_draw(uint64_t curr)
{
    uint32_t i;

    if(pb_fb) {
        i = pb_l; pb_l = ((curr >> 9) * (vidmode.framebuffer_width - 4) / pb_m) * pb_b;
        for(; i < pb_l; i++)
            pb_dot(i, fb_fg);
    }
}

/**
 * Close the progress bar
 */
PLG_API void pb_fini(void)
{
    uint32_t i, x;

    if(pb_fb)
        for(i = x = 0; x < vidmode.framebuffer_width - 2; x++, i += pb_b)
            pb_dot(i, fb_bg);
    pb_fb = NULL;
}

/**************** BIOS-specific functions ****************/

/* IMPORTANT !!! No bios_X and fw_X functions allowed to use more than 1k stack! CLang should generate a warning if they do
 * (actually Clang's stack limit is set to 512 bytes). Also these BIOS routine calls must be in the first 32k in order to work */

/**
 * Load a sector from boot drive using BIOS
 */
void bios_loadsec(uint64_t lba, void *dst)
{
    if(ST || !dst) return;
    *((uint16_t*)0x502) = 1; *((uint32_t*)0x504) = 0x600; *((uint64_t*)0x508) = lba;
    *((uint64_t*)dst) = 0;
    __asm__ __volatile__(
    /* let's see if we have master ATA IDE PIO (probably emulation, but still, it is about 100x times faster than BIOS) */
    "cmpb $0x80, (0x4ff);jne 2f;"
    "movw $0x1F7, %%dx;inb %%dx, %%al;"
    "cmpb $0, %%al;je 2f;"
    "cmpb $0xff, %%al;je 2f;"
    "1:inb %%dx, %%al;"
    "andb $0xC0, %%al;"
    "cmpb $0x40, %%al;jne 1b;"
    "movb $0x0a, %%al;movw $0x3F6, %%dx;outb %%al, %%dx;"
    "movb $0x40, %%al;movw $0x1F6, %%dx;outb %%al, %%dx;"
    "xorb %%al, %%al;movw $0x1F2, %%dx;outb %%al, %%dx;"
    "movq %%rbx, %%rax;shrq $24, %%rax;movw $0x1F3, %%dx;outb %%al, %%dx;"
    "shrq $8, %%rax;movw $0x1F4, %%dx;outb %%al, %%dx;"
    "shrq $8, %%rax;movw $0x1F5, %%dx;outb %%al, %%dx;"
    "movb $1, %%al;movw $0x1F2, %%dx;outb %%al, %%dx;"
    "movq %%rbx, %%rax;movw $0x1F3, %%dx;outb %%al, %%dx;"
    "shrq $8, %%rax;movw $0x1F4, %%dx;outb %%al, %%dx;"
    "shrq $8, %%rax;movw $0x1F5, %%dx;outb %%al, %%dx;"
    "movb $0x24, %%al;movw $0x1F7, %%dx;outb %%al, %%dx;"
    "1:inb %%dx, %%al;"
    "andb $0xC1, %%al;"
    "cmpb $0x01, %%al;je 2f;"
    "cmpb $0x40, %%al;jne 1b;"
    "movq $128, %%rcx;"
    "movw $0x1F0, %%dx;"
    "cld;rep insl;"
    "jmp 3f;"
    /* otherwise fallback to BIOS services, which means switching CPU mode and copy data, so it's slow */
    /* go BIOS mode */
    "2:pushq %%rdi;"
    "movq $0x600, %%rdi;movq $64, %%rcx;xorq %%rax, %%rax;repnz stosq;"
    "movq $16, %%rax;push %%rax;"       /* long -> compat */
    "movq $1f, %%rax;push %%rax;"
    "lretq;.code32;1:;"
    "movl %%cr0, %%eax;"                /* disable CR0.PG */
    "btcl $31, %%eax;"
    "movl %%eax, %%cr0;"
    "ljmp $8,$1f;1:;"                   /* compat -> legacy prot */
    "movl %%cr0, %%eax;"                /* disable CR0.PR */
    "andb $0xfe, %%al;"
    "movl %%eax, %%cr0;"
    "ljmp $0,$1f;.code16;1:;"           /* prot -> real */
    "xorw %%ax, %%ax;movw %%ax, %%ds;movw %%ax, %%es;movw %%ax, %%ss;"
    /* do the BIOS call */
    "movb $0x42, %%ah;"
    "movw $0x500, %%si;"
    "movb -1(%%si), %%dl;"
    "clc;int $0x13;"
    /* go back to long mode */
    "movl %%cr0, %%eax;"                /* enable CR0.PR */
    "orb $1, %%al;"
    "movl %%eax, %%cr0;"
    /* AAAAAHHHHH... we have to workaround a compiler bug... This is a dirty hack that takes advantage of the
     * fact that machine code 0xEA is the same in prot and real encoding just with different operand sizes.
     * smuggle the real mode second operand in prot mode first operand */
    ".code32;ljmp $16,$1f+0x100000;1:;"/* real -> prot */
    "movl %%cr0, %%eax;"                /* enable CR0.PG */
    "btsl $31, %%eax;"
    "movl %%eax, %%cr0;"
    "ljmp $32,$1f;.code64;1:;"          /* prot -> long */
    "movw $40, %%ax;movw %%ax, %%ds;movw %%ax, %%es;movw %%ax, %%ss;"
    /* copy data from BIOS buffer to final position */
    "xorq %%rcx, %%rcx;"
    "xorq %%rsi, %%rsi;"
    "movb $64, %%cl;"
    "movw $0x600, %%si;"
    "popq %%rdi;"
    "cmpq %%rsi, %%rdi;je 3f;"
    "repnz movsq;3:"
    ::"b"(lba),"D"(dst):"rax","rcx","rdx","rsi","memory");
}

/**
 * Set up framebuffer with VESA VBE 2.0 BIOS
 */
void bios_vbe(uint32_t width, uint32_t height, uint32_t bpp)
{
    if(ST || width < 320 || height < 200 || bpp < 15) return;
    memset((void*)0x580, 0, 128);
    edid_ptr = (uint8_t*)0x580; edid_size = 128;
    __asm__ __volatile__(
    /* go BIOS mode */
    "shlq $32, %%rcx;"
    "shlq $16, %%rbx;"
    "addq %%rbx, %%rcx;"
    "addq %%rax, %%rcx;"
    "xorq %%rax, %%rax;pushq %%rax;"
    "pushq %%rdi;"
    "pushq %%rcx;"
    "movq $16, %%rax;push %%rax;"
    "movq $1f, %%rax;push %%rax;"
    "lretq;.code32;1:;"
    "movl %%cr0, %%eax;"
    "btcl $31, %%eax;"
    "movl %%eax, %%cr0;"
    "ljmp $8,$1f;1:;"
    "movl %%cr0, %%eax;"
    "andb $0xfe, %%al;"
    "movl %%eax, %%cr0;"
    "ljmp $0,$1f;.code16;1:;"
    "xorw %%ax, %%ax;movw %%ax, %%ds;movw %%ax, %%es;movw %%ax, %%ss;"
    /* get EDID information */
    "movw $0x4f15, %%ax;"
    "movw $0x0001, %%bx;"
    "xorw %%cx, %%cx;"
    "xorw %%dx, %%dx;"
    "movw $0x580, %%di;"
    "int $0x10;"
    /* do the VBE BIOS call */
    "movw $0x600, %%di;"
    "movl $0x32454256, 0(%%di);"
    "movw $0x4f00, %%ax;"
    "pushw %%ds;pushw %%es;"
    "int $0x10;"
    "popw %%es;popw %%ds;"
    "cmpw $0x004f, %%ax;jne 2f;"
    /* copy modes list out to workaround buggy VBE BIOSes */
    "xorl %%esi, %%esi;"
    "xorl %%edi, %%edi;"
    "movw (0x60E), %%si;"
    "movw (0x610), %%ax;"
    "movw %%ax, %%ds;"
    "movw $0xA00, %%di;"
    "movw $256, %%cx;"
    "1: lodsw;"
    "cmpw $0xffff, %%ax;je 1f;"
    "cmpw $0, %%ax;je 1f;"
    "stosw;"
    "dec %%cx;jnz 1b;"
    "1:;xorw %%ax, %%ax;stosw;"
    "movw %%ax, %%ds;"
    /* iterate on modes and query info on each */
    "movw $0xffff, 20(%%esp);"          /*   stack:   */
    "movw $0xA00, %%si;"                /* 20(%esp) - best match mode */
    "1:movw $0, (0x600);"               /* 18(%esp) - best match height */
    "lodsw;"                            /* 16(%esp) - best match width */
    "orw %%ax, %%ax;jz 1f;"             /*  8(%esp) - vidmode buffer pointer */
    "movw %%ax, %%cx;"                  /*  4(%esp) - requested bpp */
    "movw $0x600, %%di;"                /*  2(%esp) - requested height */
    "movw $0x4f01, %%ax;"               /*  0(%esp) - requested width */
    "pushw %%cx;pushw %%si;pushw %%ds;pushw %%es;"
    "int $0x10;"
    "popw %%es;popw %%ds;popw %%si;popw %%cx;"
    "cmpw $0x004f, %%ax;jne 1b;"        /* mode listed, but not supported */
    "movw (0x600), %%ax;"
    "andw $0x9b, %%ax;"
    "cmpw $0x9b, %%ax;jne 1b;"          /* not the mode we're looking for. Not packed pixel linear framebuffer */
    "movb 4(%%esp), %%al;"
    "cmpb (0x619), %%al;jne 1b;"        /* bpp matches? */
    "movw (0x614), %%ax;"
    "cmpw 2(%%esp), %%ax;ja 1b;"        /* height smaller or equal than required? */
    "cmpw 18(%%esp), %%ax;jb 1b;"       /* and bigger than the last best result? */
    "movw (0x612), %%ax;"
    "cmpw 0(%%esp), %%ax;ja 1b;"        /* width smaller or equal than required? */
    "cmpw 16(%%esp), %%ax;jb 1b;"       /* and bigger than the last best result? */
    /* this is the best result we have so far, store it */
    "movw %%ax, 16(%%esp);"
    "movw (0x614), %%ax;"
    "movw %%ax, 18(%%esp);"
    "movw %%cx, 20(%%esp);"
    "jmp 1b;"
    "1:cmpw $0xffff, 20(%%esp);je 2f;"
    /* set up mode */
    "movw 20(%%esp), %%bx;"
    "orw $0x6000, %%bx;"
    "movw $0x4f02, %%ax;"
    "pushw %%cx;pushw %%si;pushw %%ds;pushw %%es;"
    "int $0x10;"
    "popw %%es;popw %%ds;popw %%si;popw %%cx;"
    "cmpw $0x004f, %%ax;jne 2f;"        /* not worked, so don't store it */
    /* looks good, store vidmode */
    "movw 20(%%esp), %%cx;"
    "movw $0x600, %%di;"
    "movw $0x4f01, %%ax;"
    "pushw %%cx;pushw %%si;pushw %%ds;pushw %%es;"
    "int $0x10;"
    "popw %%es;popw %%ds;popw %%si;popw %%cx;"
    "cmpw $0x004f, %%ax;jne 2f;"        /* should never happen */
    "movl 8(%%esp), %%edi;"
    "movl (0x628), %%eax;"
    "movl %%eax, 8(%%edi);"             /* vidmode.framebuffer_addr lo */
    "movl (0x62c), %%eax;"
    "movl %%eax, 12(%%edi);"            /* vidmode.framebuffer_addr hi */
    "movw (0x610), %%ax;"
    "movw %%ax, 16(%%edi);"             /* vidmode.framebuffer_pitch */
    "movw (0x612), %%ax;"
    "movw %%ax, 20(%%edi);"             /* vidmode.framebuffer_width */
    "movw (0x614), %%ax;"
    "movw %%ax, 24(%%edi);"             /* vidmode.framebuffer_height */
    "movb (0x619), %%al;"
    "movb %%al, 28(%%edi);"             /* vidmode.framebuffer_bpp */
    "movw (0x61F), %%ax;xchgb %%al, %%ah;"
    "movw %%ax, 32(%%edi);"             /* vidmode.framebuffer_red_field_position + mask */
    "movw (0x621), %%ax;xchgb %%al, %%ah;"
    "movw %%ax, 34(%%edi);"             /* vidmode.framebuffer_green_field_position + mask */
    "movw (0x623), %%ax;xchgb %%al, %%ah;"
    "movw %%ax, 36(%%edi);"             /* vidmode.framebuffer_blue_field_position + mask */
    "2:;"
    /* go back to long mode */
    "movl %%cr0, %%eax;"
    "orb $1, %%al;"
    "movl %%eax, %%cr0;"
    ".code32;ljmp $16,$1f+0x100000;1:;"
    "movl %%cr0, %%eax;"
    "btsl $31, %%eax;"
    "movl %%eax, %%cr0;"
    "ljmp $32,$1f;.code64;1:;"
    "movw $40, %%ax;movw %%ax, %%ds;movw %%ax, %%es;movw %%ax, %%ss;"
    "addq $24, %%rsp;"
    ::"a"(width),"b"(height),"c"(bpp),"D"(&vidmode):"rsi","rdx","memory");
    if(!vidmode.framebuffer_addr) printf(" VESA: %s\n", "no framebuffer");
#ifdef CONSOLE_FB
    else fb_x = fb_y = 4;
#endif
}

/**
 * Get E820 memory map
 */
int bios_e820(multiboot_mmap_entry_t *dst)
{
    int ret = 0, i, j;
    uint64_t top;
    multiboot_mmap_entry_t tmp;

    if(ST || !dst) return 0;
    __asm__ __volatile__(
    /* go BIOS mode */
    "pushq %%rdi;xorq %%rdi, %%rdi;pushq $0;"
    "movq $16, %%rax;push %%rax;"
    "movq $1f, %%rax;push %%rax;"
    "lretq;.code32;1:;"
    "movl %%cr0, %%eax;"
    "btcl $31, %%eax;"
    "movl %%eax, %%cr0;"
    "ljmp $8,$1f;1:;"
    "movl %%cr0, %%eax;"
    "andb $0xfe, %%al;"
    "movl %%eax, %%cr0;"
    "ljmp $0,$1f;.code16;1:;"
    "xorw %%ax, %%ax;movw %%ax, %%ds;movw %%ax, %%es;movw %%ax, %%ss;"
    /* do the BIOS call */
    "clc;xorl %%ebx, %%ebx;movl $0x600, %%edi;"
    "1:movl $0xE820, %%eax;"
    "movl $0x534d4150, %%edx;"
    "xorl %%ecx, %%ecx;"
    "movb $20, %%cl;"
    "int $0x15;jc 1f;"
    "addl $20, %%edi;xorl %%eax, %%eax;stosl;"
    "incl 0(%%esp);"
    "or %%ebx, %%ebx;jnz 1b;"
    /* go back to long mode */
    "1:movl %%cr0, %%eax;"
    "orb $1, %%al;"
    "movl %%eax, %%cr0;"
    ".code32;ljmp $16,$1f+0x100000;1:;"
    "movl %%cr0, %%eax;"
    "btsl $31, %%eax;"
    "movl %%eax, %%cr0;"
    "ljmp $32,$1f;.code64;1:;"
    "movw $40, %%ax;movw %%ax, %%ds;movw %%ax, %%es;movw %%ax, %%ss;"
    "movq %%rdi, %%rcx;movq $0x600, %%rsi;subq %%rsi, %%rcx;"
    "popq %%rbx;popq %%rdi;repnz movsb;movq %%rbx, %%rax;"
    :"=a"(ret):"D"(dst):"rbx","rcx","rdx","rsi","memory");
    /* make sure of it that the memory map is sorted. Should be, so bubble-sort is affordable here */
    for(i = 1; i < ret; i++) {
        for(j = i; j > 0 && dst[j].base_addr < dst[j - 1].base_addr; j--) {
            memcpy(&tmp, &dst[j - 1], sizeof(multiboot_mmap_entry_t));
            memcpy(&dst[j - 1], &dst[j], sizeof(multiboot_mmap_entry_t));
            memcpy(&dst[j], &tmp, sizeof(multiboot_mmap_entry_t));
        }
        top = dst[i].base_addr + dst[i].length;
        if(dst[i].type == MULTIBOOT_MEMORY_AVAILABLE) {
            if(top > ram) ram = top;
            /* since memory allocation also uses file_buf, we replace it to the first free slot's half
             * this way segments can be loaded to 1M without interference */
            if(dst[i].base_addr <= file_base && file_base < top) {
                file_base = (dst[i].base_addr + dst[i].length / 2 + 2*1024*1024-1) & ~(2*1024*1024-1);
                if(file_base > 64*1024*1024) file_base = 64*1024*1024;
            }
        }
    }
    if(ret < 1) printf(" E820: %s\n", "unable to get memory map");
    return ret;
}

/**
 * Sleep 1 usec
 */
void bios_sleep(void)
{
            /* NOTE: this should be the right solution, as it only relies on PS/2 alone and needs nothing else. However qemu's
             * PS/2 emulation is buggy, the oscillation is host freq dependent and not constant as it is on a real hardware. */
#if 0
            /* wait a bit, PS/2 control port's bit 4 is oscillating at 15 usecs, use that (67 * 15 = 1005 usecs) */
            __asm__ __volatile__ ("inb $0x61,%%al;andb $0x10,%%al;movb %%al,%%ah;movw $67, %%cx;"
            "1:;inb $0x61,%%al;andb $0x10,%%al;cmpb %%al,%%ah;je 1b;movb %%al,%%ah;nop;pause;dec %%cx;jnz 1b;" : : : "rax", "rbx", "rcx", "rdx");
#else
            /* wait a bit, polling the PIT latch for delay. 1000 usec = 1,193,182 Hz / 1000 ms = 1193 */
            __asm__ __volatile__ ("movw $1193, %%cx;"                                                       /* cx = ticks to wait */
            "xor %%al,%%al;outb %%al,$0x43;inb $0x40,%%al;movb %%al,%%bl;inb $0x40,%%al;movb %%al,%%bh;"    /* bx = last counter */
            "1:xor %%al,%%al;outb %%al,$0x43;inb $0x40,%%al;movb %%al,%%ah;inb $0x40,%%al;xchgb %%al,%%ah;" /* ax = current counter */
            "nop;pause;subw %%ax,%%bx;subw %%bx,%%cx;movw %%ax,%%bx;jae 1b;" /* cx -= (bx - ax); bx = ax; while(cx > 0); */
             : : : "rax", "rbx", "rcx");
#endif
}

/**
 * Allocate and zero out a page on BIOS
 */
uint64_t bios_alloc(uint32_t num)
{
    uint64_t page = file_buf;
    file_buf += num * 4096;
    memset((void*)page, 0, num * 4096);
    return page;
}

/**
 * Get the next cluster from FAT
 */
uint32_t bios_nextclu(uint32_t clu)
{
    uint64_t i;

    if(ST || clu < 2 || clu >= 0x0FFFFFF8) return 0;
    if(clu < fat_cache || clu > fat_cache + 1023) {
        fat_cache = clu & ~1023;
        for(i = 0; i < 8; i++) bios_loadsec(fat_lba + (fat_cache >> 7) + i, &fat[i << 7]);
    }
    clu = fat[clu - fat_cache];
    return clu < 2 || clu >= 0x0FFFFFF8 ? 0 : clu;
}

/**
 * Open a file on BIOS
 */
int bios_open(uint16_t *fn)
{
    uint64_t lba;
    uint32_t clu = bpb->rc, c, l;
    int i, n = 0, m = 0, rd = 0;
    uint8_t secleft = 0, *dir = data + sizeof(data), *plgs = plgids, *p;
    uint16_t *u, *s = fn, a, b;

    if(ST || !root_dir || !fn || !*fn) return 0;
    file_size = file_clu = 0;
    memset(lfn, 0, sizeof(lfn));
    while(1) {
        /* have we reached the end of the sector? */
        if(dir >= data + sizeof(data)) {
            if(secleft) { secleft--; lba++; }
            else {
                if(clu < 2 || clu >= 0x0FFFFFF8) return 0;
                secleft = bpb->spc - 1;
                lba = clu * bpb->spc + data_lba;
                clu = bios_nextclu(clu);
            }
            bios_loadsec(lba, &data);
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
                if(rddir && rd) {
                    /* read directory entries */
                    c = (dir[0x15] << 24) | (dir[0x14] << 16) | (dir[0x1B] << 8) | dir[0x1A];
                    if(c >= 2 && c < 0x0FFFFFF8 && plgs + 512 < plgids + 0x40000) {
                        bios_loadsec(c * bpb->spc + data_lba, plgs);
                        if(!memcmp(plgs, PLG_MAGIC, 4) && (((plg_hdr_t*)plgs)->nid || ((plg_hdr_t*)plgs)->type == PLG_T_TAG) &&
                          ((plg_hdr_t*)plgs)->arch == EM_X86_64) {
                            p = plgs; l = ((plg_hdr_t*)plgs)->nid * sizeof(plg_id_t);
                            plgs[10] = ((plg_hdr_t*)plgs)->type;
                            plgs[11] = ((plg_hdr_t*)plgs)->nid;
                            memcpy(plgs + 12, plgs + sizeof(plg_hdr_t), l);
                            plgs = ucs2(plgs + 12 + l, lfn);
                            *((uint16_t*)p) = plgs - p; *((uint64_t*)(p + 2)) = 0;
                        }
                        plgs[0] = plgs[1] = 0;
                    }
                } else {
                    /* filename match? */
                    if(*s == '/') s++;
                    for(i = 0; lfn[i] && s[i] && s[i] != '/'; i++) {
                        a = lfn[i]; if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
                        b = s[i]; if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
                        if(a != b) break;
                    }
                    if(!lfn[i]) {
                        clu = (dir[0x15] << 24) | (dir[0x14] << 16) | (dir[0x1B] << 8) | dir[0x1A];
                        if(clu < 2 || clu >= 0x0FFFFFF8) return 0;
                        /* is this a directory? */
                        if(dir[0xB] & 0x10) {
                            if(s[i] != '/') return 0;
                            /* go to subdirectory */
                            s += i + 1; n = m = secleft = 0; dir = data + sizeof(data);
                            if(!*s) { rd = 1; }
                            continue;
                        } else {
                            /* no, it's a file, then we have located what we were looking for */
                            if(s[i]) return 0;
                            file_clu = clu;
                            file_size = (dir[0x1F] << 24) | (dir[0x1E] << 16) | (dir[0x1D] << 8) | dir[0x1C];
                            break;
                        }
                    }
                }
            }
        }
        dir += 32;
    }
    return 1;
}

/**
 * Read data from file on BIOS
 */
uint64_t bios_read(uint64_t offs, uint64_t size, void *buf)
{
    uint64_t lba = 0, rem, o;
    uint32_t clu = file_clu, nc, ns = 0, os = 0, rs = 512;
    uint8_t secleft = 0;

    if(ST || file_clu < 2 || offs >= file_size || !size || !buf) return 0;
    if(offs + size > file_size) size = file_size - offs;
    rem = size;

    pb_init(size);
    if(offs) {
        nc = offs / (bpb->spc << 9); o = offs % (bpb->spc << 9);
        ns = o >> 9; os = o & 0x1ff; rs = 512 - os;
        if(nc) { while(nc-- && clu) { clu = bios_nextclu(clu); } if(!clu) return 0; }
        secleft = bpb->spc - ns - 1;
        lba = clu * bpb->spc + ns - 1 + data_lba;
    }
    while(rem) {
        if(secleft) { secleft--; lba++; }
        else {
            if(!clu) break;
            secleft = bpb->spc - 1;
            lba = clu * bpb->spc + data_lba;
            clu = bios_nextclu(clu);
        }
        if(rs > rem) rs = rem;
        if(rs < 512) {
            bios_loadsec(lba, data);
            memcpy(buf, data + os, rs); os = 0;
        } else {
            bios_loadsec(lba, buf);
            if(os) { memcpy(buf, buf + os, rs); os = 0; }
        }
        buf += rs; rem -= rs; rs = 512;
        pb_draw(size - rem);
    }
    pb_fini();
    return (size - rem);
}

/**
 * Close file on BIOS
 */
void bios_close(void)
{
    file_clu = 0;
}

/**
 * Generate tags for system tables on BIOS
 */
void bios_systables(void)
{
    int sm = 0, rp = 0, i;
    uint8_t *s, chk;

    for(s = (uint8_t*)0x9A000; s < (uint8_t*)0x100000 && !(sm & rp); s += 16) {
        if(!memcmp(s, "_SM_", 4)) {
            for(chk = 0, i = 0; (uint32_t)i < (uint32_t)s[5]; i++) chk += s[i];
            if(!chk) {
                memset(tags_ptr, 0, sizeof(multiboot_tag_smbios_t));
                ((multiboot_tag_smbios_t*)tags_ptr)->type = MULTIBOOT_TAG_TYPE_SMBIOS;
                ((multiboot_tag_smbios_t*)tags_ptr)->size = sizeof(multiboot_tag_smbios_t) + (uint32_t)s[5];
                ((multiboot_tag_smbios_t*)tags_ptr)->major = s[7];
                ((multiboot_tag_smbios_t*)tags_ptr)->minor = s[8];
                memcpy(((multiboot_tag_smbios_t*)tags_ptr)->tables, s, (uint32_t)s[5]);
                tags_ptr += (((multiboot_tag_smbios_t*)tags_ptr)->size + 7) & ~7;
            }
            sm = 1;
        } else
        if(!memcmp(s, "RSD PTR ", 8)) {
            if(s[15] < 2) {
                ((multiboot_tag_old_acpi_t*)tags_ptr)->type = MULTIBOOT_TAG_TYPE_ACPI_OLD;
                ((multiboot_tag_old_acpi_t*)tags_ptr)->size = sizeof(multiboot_tag_old_acpi_t) + 24;
                memcpy(((multiboot_tag_old_acpi_t*)tags_ptr)->rsdp, s, 24);
                tags_ptr += 32;
            } else {
                ((multiboot_tag_new_acpi_t*)tags_ptr)->type = MULTIBOOT_TAG_TYPE_ACPI_NEW;
                ((multiboot_tag_new_acpi_t*)tags_ptr)->size = sizeof(multiboot_tag_new_acpi_t) + 36;
                memcpy(((multiboot_tag_new_acpi_t*)tags_ptr)->rsdp, s, 36);
                tags_ptr += 48;
            }
            rsdp_ptr = s;
            rp = 1;
        }
    }
}

/**
 * Initialize BIOS related things
 */
void bios_init(void)
{
    guid_t espGuid = EFI_PART_TYPE_EFI_SYSTEM_PART_GUID;
    uint64_t i, j, k, l, n;

    /* finish up the page tables (up to 5G to be safe) */
    /* We couldn't do this sooner, as this overwrites the PMBR code at 0x7C00 */
    memset((void*)0x1008, 0, 0xFF8); memset((void*)0x2008, 0, 0xFF8);
    for(i = 0; i < 4; i++)      *((uint64_t*)(0x2008 + i * 8)) = 0x4003 + i * 0x1000;
    for(i = 0; i < 2048; i++)   *((uint64_t*)(0x4000 + i * 8)) = (i + 512) * 2 * 1024 * 1024 + 0x83;
    __asm__ __volatile__("movq %%rax, %%cr3"::"a"(0x1000):);
    pt = (uint64_t*)0x1000;
    tags_buf = (uint8_t*)0x20000; plgids = NULL; file_buf = file_base = 16*1024*1024;
    bios_vbe(800, 600, 32);
    if(!vidmode.framebuffer_addr) bios_vbe(640, 480, 32);

    /* get boot partition's root directory */
    bios_loadsec(1, &vbr);
    if(!memcmp(&vbr, EFI_PTAB_HEADER_ID, 8)) {
        /* found GPT */
        j = ((gpt_header_t*)&vbr)->SizeOfPartitionEntry;
        l = ((gpt_header_t*)&vbr)->PartitionEntryLBA;
        n = ((gpt_header_t*)&vbr)->NumberOfPartitionEntries;
        /* look for ESP */
        for(k = 0; n && !root_dir; k++) {
            bios_loadsec(l + k, &vbr);
            for(i = 0; n && i + j <= 512; i += j, n--) {
                /* does ESP type match? */
                if(!memcmp(&((gpt_entry_t*)&vbr[i])->PartitionTypeGUID, &espGuid, sizeof(guid_t))) {
                    root_dir = (void*)(((gpt_entry_t*)&vbr[i])->StartingLBA);
                    break;
                }
            }
        }
    } else {
        /* fallback to MBR partitioning scheme */
        bios_loadsec(0, &vbr);
        if(vbr[510] == 0x55 && vbr[511] == 0xAA)
            for(i = 0x1c0; i < 510; i += 16)
                if(vbr[i - 2] == 0x80/*active*/ && (vbr[i + 2] == 0xC/*FAT32*/ || vbr[i + 2] == 0xEF/*ESP*/)) {
                    root_dir = (void*)(uint64_t)(*((uint32_t*)&vbr[i + 6]));
                    break;
                }
    }
    /* we shamelessly reuse the pointer to store the boot partition's start LBA, because later we use that as
     * a flag to see if we have found a file system, otherwise we add it to data_lba and never use it again */
    if(root_dir) {
        bios_loadsec((uint64_t)root_dir, &vbr);
        bpb = (esp_bpb_t*)&vbr;
        if(vbr[510] != 0x55 || vbr[511] != 0xAA || bpb->bps != 512 || !bpb->spc || bpb->spf16 || !bpb->spf32)
            root_dir = NULL;
        else {
            /* calculate the LBA address of the FAT and the first data sector */
            fat_lba = bpb->rsc + (uint64_t)root_dir;
            data_lba = bpb->spf32 * bpb->nf + bpb->rsc - 2 * bpb->spc + (uint64_t)root_dir;
            /* load the beginning of the FAT into the cache */
            for(i = 0; i < 8; i++) bios_loadsec(fat_lba + i, &fat[i << 7]);
            fat_cache = 0;
        }
    }
}

/**
 * Find the root partition
 */
int bios_findroot(void)
{
    uint64_t i, j, k, l, n;

    for(*((uint8_t*)0x4ff) = 0x80; *((uint8_t*)0x4ff) < 0x90; *((uint8_t*)0x4ff) += 1) {
        bios_loadsec(1, &data);
        if(!memcmp(&data, EFI_PTAB_HEADER_ID, 8)) {
            j = ((gpt_header_t*)&data)->SizeOfPartitionEntry;
            l = ((gpt_header_t*)&data)->PartitionEntryLBA;
            n = ((gpt_header_t*)&data)->NumberOfPartitionEntries;
            for(k = 0; n; k++) {
                bios_loadsec(l + k, &data);
                for(i = 0; n && i + j <= 512; i += j, n--) {
                    if(!memcmp(&((gpt_entry_t*)&data[i])->UniquePartitionGUID, &rootuuid, sizeof(guid_t))) {
                        root_buf = (uint8_t*)bios_alloc(32); if(!root_buf) return 0;
                        root_lba = ((gpt_entry_t*)&data[i])->StartingLBA;
                        root_siz = ((gpt_entry_t*)&data[i])->EndingLBA - root_lba + 1;
                        for(i = 0; i < 256; i++)
                            bios_loadsec(root_lba + i, (void*)(root_buf + (i << 9)));
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

/* we can't include this sooner, because the routines above must be in the first 32k. This is because we must
 * access those from real mode, and we're loaded at 0x8000, and the real mode segment's limit is 0xFFFF bytes. */
#include "inflate.h"

/**************** EFI-specific functions ****************/

/**
 * Calculate bitoffsets from mask
 */
static void efi_bitmask(uint8_t *p, uint8_t *s, uint32_t m)
{
    for(*p = 0; !(m & (1 << *p)); (*p)++);
    for(*s = 0; m & (1 << (*p + *s)); (*s)++);
}

/**
 * Set up framebuffer with UEFI GOP
 */
void efi_gop(uint32_t width, uint32_t height, uint32_t bpp)
{
    guid_t gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    guid_t edid1Guid = EFI_EDID_ACTIVE_GUID;
    guid_t edid2Guid = EFI_EDID_DISCOVERED_GUID;
    efi_gop_t *gop = NULL;
    efi_gop_mode_info_t *info = NULL;
    efi_edid_t *edid = NULL;
    efi_status_t status;
    uintn_t isiz = sizeof(efi_gop_mode_info_t), i = sizeof(data);
    uint32_t m, b = 0, bw = 0, bh = 0, bm = -1U;
    efi_handle_t *handles = (efi_handle_t*)data;

    memset(handles, 0, i);
    if(!ST || !BS || width < 320 || height < 200 || bpp < 15 || EFI_ERROR(BS->LocateHandle(ByProtocol, &gopGuid, NULL, &i, handles)))
        return;
    if(!EFI_ERROR(BS->OpenProtocol(handles[0], &gopGuid, (void**)&gop, IM, 0, 2)) && gop) {
        /* we got the interface, get current mode */
        status = gop->QueryMode(gop, gop->Mode ? gop->Mode->Mode : 0, &isiz, &info);
        if(EFI_ERROR(status) || !gop->Mode)
            status = gop->SetMode(gop, 0);
        if(!EFI_ERROR(status)) {
            /* iterate on modes and find the largest screen with the requested bpp */
            for(i = 0; i < gop->Mode->MaxMode; i++) {
                if(EFI_ERROR(gop->QueryMode(gop, i, &isiz, &info)) || info->PixelFormat > PixelBitMask) continue;
                switch(info->PixelFormat) {
                    case PixelRedGreenBlueReserved8BitPerColor:
                    case PixelBlueGreenRedReserved8BitPerColor: b = 32; break;
                    default:
                        for(m = info->PixelInformation.RedMask | info->PixelInformation.GreenMask | info->PixelInformation.BlueMask,
                            b = 32; b > 0 && !(m & (1 << (b - 1))); b--);
                    break;
                }
                if(bpp == b && info->HorizontalResolution <= width && info->VerticalResolution <= height &&
                  info->HorizontalResolution > bw && info->VerticalResolution > bh) {
                    bm = i; bw = info->HorizontalResolution; bh = info->VerticalResolution;
                }
            }
        }
        /* try the best mode that we've found */
        if(bm != -1U) {
            if(!EFI_ERROR(gop->SetMode(gop, bm))) {
#ifdef CONSOLE_FB
                fb_x = fb_y = 4;
#endif
                status = gop->QueryMode(gop, gop->Mode ? gop->Mode->Mode : 0, &isiz, &info);
                vidmode.framebuffer_red_mask_size = vidmode.framebuffer_green_mask_size = vidmode.framebuffer_blue_mask_size = 8;
                vidmode.framebuffer_green_field_position = 8; b = 32;
                switch(info->PixelFormat) {
                    case PixelRedGreenBlueReserved8BitPerColor:
                        vidmode.framebuffer_red_field_position = 0;
                        vidmode.framebuffer_blue_field_position = 16;
                    break;
                    case PixelBlueGreenRedReserved8BitPerColor:
                        vidmode.framebuffer_red_field_position = 16;
                        vidmode.framebuffer_blue_field_position = 0;
                    break;
                    default:
                        for(m = info->PixelInformation.RedMask | info->PixelInformation.GreenMask | info->PixelInformation.BlueMask,
                            b = 32; b > 0 && !(m & (1 << (b - 1))); b--);
                        efi_bitmask(&vidmode.framebuffer_red_field_position, &vidmode.framebuffer_red_mask_size,
                            info->PixelInformation.RedMask);
                        efi_bitmask(&vidmode.framebuffer_green_field_position, &vidmode.framebuffer_green_mask_size,
                            info->PixelInformation.GreenMask);
                        efi_bitmask(&vidmode.framebuffer_blue_field_position, &vidmode.framebuffer_blue_mask_size,
                            info->PixelInformation.BlueMask);
                    break;
                }
                vidmode.framebuffer_addr = gop->Mode->FrameBufferBase;
                vidmode.framebuffer_pitch = info->PixelsPerScanLine * ((b + 7) >> 3);
                vidmode.framebuffer_width = info->HorizontalResolution;
                vidmode.framebuffer_height = info->VerticalResolution;
                vidmode.framebuffer_bpp = b;
                /* get EDID information. Due to firmware bug, this only works after QueryMode was called */
                if(((!EFI_ERROR(BS->HandleProtocol(handles[0], &edid1Guid, (void **)&edid)) && edid && edid->SizeOfEdid && edid->Edid) ||
                    (!EFI_ERROR(BS->HandleProtocol(handles[0], &edid2Guid, (void **)&edid)) && edid && edid->SizeOfEdid && edid->Edid))) {
                        edid_ptr = edid->Edid; edid_size = edid->SizeOfEdid;
                }
            }
        }
        if(!vidmode.framebuffer_addr) { printf(" GOP: %s\n", "no framebuffer"); }
    }
}

/**
 * Get EFI memory map
 */
int efi_memmap(multiboot_mmap_entry_t *dst)
{
    int ret = 0, i, j;
    uint64_t top;
    multiboot_mmap_entry_t tmp;
    efi_memory_descriptor_t *memory_map = NULL, *mement;
    uintn_t memory_map_size = 0, map_key = 0, desc_size = 0;

    if(!ST || !BS) return 0;
    /* get memory map size */
    if(BS->GetMemoryMap(&memory_map_size, NULL, &map_key, &desc_size, NULL) != EFI_BUFFER_TOO_SMALL || !memory_map_size) goto err;
    /* allocate buffer. This might increase the memory map's size */
    memory_map_size += 4 * desc_size;
    if(EFI_ERROR(BS->AllocatePool(EfiLoaderData, memory_map_size, (void**)&memory_map)) || !memory_map) goto err;
    /* get memory map */
    if(EFI_ERROR(BS->GetMemoryMap(&memory_map_size, memory_map, &map_key, &desc_size, NULL))) { BS->FreePool(memory_map); goto err; }
    if(dst) {
        /* convert to Multiboot2 memory entry tags */
        for(mement = memory_map; (uint8_t*)mement < (uint8_t*)memory_map + memory_map_size;
          mement = NextMemoryDescriptor(mement, desc_size), ret++) {
            dst[ret].base_addr = mement->PhysicalStart;
            dst[ret].length = mement->NumberOfPages << 12;
            dst[ret].reserved = mement->Type;
            dst[ret].type = ((mement->Type > EfiReservedMemoryType && mement->Type < EfiRuntimeServicesCode) ||
                mement->Type == EfiConventionalMemory ? MULTIBOOT_MEMORY_AVAILABLE :
                (mement->Type == EfiUnusableMemory ? MULTIBOOT_MEMORY_BADRAM :
                (mement->Type == EfiACPIReclaimMemory ? MULTIBOOT_MEMORY_ACPI_RECLAIMABLE :
                (mement->Type == EfiACPIMemoryNVS ? MULTIBOOT_MEMORY_NVS : MULTIBOOT_MEMORY_RESERVED))));
        }
        /* make sure of it that the memory map is sorted. Should be, so bubble-sort is affordable here */
        for(i = 1; i < ret; i++)
            for(j = i; j > 0 && dst[j].base_addr < dst[j - 1].base_addr; j--) {
                memcpy(&tmp, &dst[j - 1], sizeof(multiboot_mmap_entry_t));
                memcpy(&dst[j - 1], &dst[j], sizeof(multiboot_mmap_entry_t));
                memcpy(&dst[j], &tmp, sizeof(multiboot_mmap_entry_t));
            }
    } else {
        /* just iterate through to find the top of memory, aka the amount of RAM we have */
        for(mement = memory_map; (uint8_t*)mement < (uint8_t*)memory_map + memory_map_size;
          mement = NextMemoryDescriptor(mement, desc_size)) {
            top = mement->PhysicalStart + (mement->NumberOfPages << 12);
            if(mement->Type == EfiConventionalMemory && top > ram) ram = top;
        }
    }
    BS->FreePool(memory_map);
err:if(dst && ret < 1) printf(" UEFI: %s\n", "unable to get memory map");
    return ret;
}

/**
 * Allocate and zero out a page on UEFI
 */
uint64_t efi_alloc(uint32_t num)
{
    uint64_t page = 0;
    /* this must work before and *after* ExitBootServices as well */
    if(BS && BS->AllocatePages) {
        if(EFI_ERROR(BS->AllocatePages(AllocateAnyPages, EfiLoaderData, num, (efi_physical_address_t*)&page))) page = 0;
    } else {
        if(!file_buf) file_buf = 0x100000;
        page = file_buf; file_buf += num * 4096;
    }
    if(page) memset((void*)page, 0, num * 4096);
    return page;
}

/**
 * Open a file on UEFI
 */
int efi_open(uint16_t *fn)
{
    guid_t infGuid = EFI_FILE_INFO_GUID;
    uintn_t fsiz = (uintn_t)sizeof(efi_file_info_t);
    int i;

    if(!ST || !root_dir || !fn || !*fn) return 0;
    for(i = 0; fn[i]; i++) if(fn[i] == '/') fn[i] = '\\';
    if(EFI_ERROR(root_dir->Open(root_dir, &f, fn, EFI_FILE_MODE_READ, 0))) { f = NULL; file_size = 0; return 0; }
    file_size = EFI_ERROR(f->GetInfo(f, &infGuid, &fsiz, &info)) ? 0 : (uint64_t)info.FileSize;
    return 1;
}

/**
 * Read data from file on UEFI
 */
uint64_t efi_read(uint64_t offs, uint64_t size, void *buf)
{
    efi_status_t status;
    uint64_t blksize, curr;

    if(!ST || !f || offs >= file_size || !size || !buf) return 0;
    if(offs + size > file_size) size = file_size - offs;
    status = f->SetPosition(f, offs);
    if(EFI_ERROR(status)) return 0;
    /* calculate how many bytes to read for one pixel progress. If zero, then no progress bar, read everything in one batch */
    if(!(blksize = pb_init(size))) blksize = size;
    for(curr = 0; !EFI_ERROR(status) && curr < size; curr += blksize) {
        if(size - curr < blksize) blksize = size - curr;
        status = f->Read(f, &blksize, buf + curr);
        pb_draw(curr);
    }
    return EFI_ERROR(status) ? 0 : size;
}

/**
 * Close file on UEFI
 */
void efi_close(void)
{
    if(ST && f) f->Close(f);
    f = NULL;
}

/**
 * Load list of available plugins
 */
void efi_loadplugins(void)
{
    efi_file_handle_t *dir;
    uint64_t bs;
    uint8_t *plgs, *s, *d, *p;

    plgids = NULL;
    if(!ST || !root_dir || !f || !(info.Attribute & 0x10)) return;
    /* no realloc in UEFI, so we have to allocate the memory in advance */
    if(EFI_ERROR(BS->AllocatePool(EfiLoaderData, 0x40000, (void**)&plgids)) || !plgids) return;
    plgs = plgids; dir = f; for(s = (uint8_t*)fn; *s; s++);
    memset(plgs, 0, 0x40000);
    /* iterate on directory entries */
    while(plgs + 512 < plgids + 0x40000) {
        bs = sizeof(efi_file_info_t);
        if(EFI_ERROR(dir->Read(dir, &bs, &dirent)) || !bs) break;
        if(dirent.Attribute & 0x10) continue;
        d = ucs2(s, dirent.FileName);
        if(fw_open(fn)) {
            efi_read(0, 512, plgs);
            efi_close();
            if(!memcmp(plgs, PLG_MAGIC, 4) && (((plg_hdr_t*)plgs)->nid || ((plg_hdr_t*)plgs)->type == PLG_T_TAG) &&
              ((plg_hdr_t*)plgs)->arch == EM_X86_64) {
                p = plgs;
                plgs[10] = ((plg_hdr_t*)plgs)->type;
                plgs[11] = ((plg_hdr_t*)plgs)->nid;
                memcpy(plgs + 12, plgs + sizeof(plg_hdr_t), ((plg_hdr_t*)plgs)->nid * sizeof(plg_id_t));
                plgs += 12 + ((plg_hdr_t*)plgs)->nid * sizeof(plg_id_t);
                memcpy(plgs, s, d - s);
                plgs += d - s;
                *((uint16_t*)p) = plgs - p; *((uint64_t*)(p + 2)) = 0;
            }
            memset(plgs, 0, 10);
        }
    }
    dir->Close(dir);
}

/**
 * Generate tags for system tables on UEFI
 */
void efi_systables(void)
{
    guid_t smGuid = SMBIOS_TABLE_GUID, r1Guid = ACPI_TABLE_GUID, r2Guid = ACPI_20_TABLE_GUID;
    efi_configuration_table_t *tbl;
    uint32_t i;
    uint8_t *s;

    if(!ST || !tags_ptr) return;
    for(i = 0, tbl = ST->ConfigurationTable; i < ST->NumberOfTableEntries; i++, tbl++) {
        if(!memcmp(&tbl->VendorGuid, &smGuid, sizeof(guid_t))) {
            s = tbl->VendorTable;
            memset(tags_ptr, 0, sizeof(multiboot_tag_smbios_t));
            ((multiboot_tag_smbios_t*)tags_ptr)->type = MULTIBOOT_TAG_TYPE_SMBIOS;
            ((multiboot_tag_smbios_t*)tags_ptr)->size = sizeof(multiboot_tag_smbios_t) + (uint32_t)s[5];
            ((multiboot_tag_smbios_t*)tags_ptr)->major = s[7];
            ((multiboot_tag_smbios_t*)tags_ptr)->minor = s[8];
            memcpy(((multiboot_tag_smbios_t*)tags_ptr)->tables, s, (uint32_t)s[5]);
            tags_ptr += (((multiboot_tag_smbios_t*)tags_ptr)->size + 7) & ~7;
        } else
        if(!memcmp(&tbl->VendorGuid, &r1Guid, sizeof(guid_t)) && !rsdp_ptr) {
            s = tbl->VendorTable;
            ((multiboot_tag_old_acpi_t*)tags_ptr)->type = MULTIBOOT_TAG_TYPE_ACPI_OLD;
            ((multiboot_tag_old_acpi_t*)tags_ptr)->size = sizeof(multiboot_tag_old_acpi_t) + 24;
            memcpy(((multiboot_tag_old_acpi_t*)tags_ptr)->rsdp, s, 24);
            tags_ptr += 32;
            rsdp_ptr = s;
        } else
        if(!memcmp(&tbl->VendorGuid, &r2Guid, sizeof(guid_t))) {
            s = tbl->VendorTable;
            ((multiboot_tag_new_acpi_t*)tags_ptr)->type = MULTIBOOT_TAG_TYPE_ACPI_NEW;
            ((multiboot_tag_new_acpi_t*)tags_ptr)->size = sizeof(multiboot_tag_new_acpi_t) + 36;
            memcpy(((multiboot_tag_new_acpi_t*)tags_ptr)->rsdp, s, 36);
            tags_ptr += 48;
            rsdp_ptr = s;
        }
    }
}

/**
 * Initialize UEFI related things
 */
void efi_init(void)
{
    efi_loaded_image_protocol_t *LIP = NULL;
    efi_simple_file_system_protocol_t *sfs = NULL;
    guid_t dppGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;
    guid_t lipGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    guid_t sfsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    int i;

    BS = ST->BootServices;
    /* we don't need this buffer, we just need this area to be marked as used in the memory map, so that
     * subsequent allocations won't pollute it. If UEFI already occupies this area, that sucks, there's
     * nothing we can do about it and would most likely result in "memory already in use" errors in
     * fw_loadseg. Otherwise this memory is freed in fw_fini. */
    hack_buf = 0x100000;
    if(EFI_ERROR(BS->AllocatePages(AllocateAddress, EfiLoaderData, 1024, (efi_physical_address_t*)&hack_buf))) hack_buf = 0;
    efi_gop(800, 600, 32);
    if(!vidmode.framebuffer_addr) efi_gop(640, 480, 32);
    bootdev = NULL;
    if(IM && BS && BS->HandleProtocol) {
        BS->HandleProtocol(IM, &lipGuid, (void **)&LIP);
        if(!EFI_ERROR(BS->HandleProtocol(LIP->DeviceHandle, &sfsGuid, (void **)&sfs))) {
            /* get boot partition's root directory */
            if(EFI_ERROR(sfs->OpenVolume(sfs, &root_dir))) root_dir = NULL;
        }
        /* get the ESP's device path, might be altered in efi_findroot */
        if(EFI_ERROR(BS->HandleProtocol(LIP->DeviceHandle, &dppGuid, (void **)&bootdev))) bootdev = NULL;
    }
    /* if this fails, that sucks, because we don't have a console yet. We'll report it later */
    pt = NULL;
    if(EFI_ERROR(BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 8, (efi_physical_address_t*)&pt))) pt = NULL;
    else {
        /* otherwise set up the same page tables we have with BIOS, but don't active it yet */
        memset(pt, 0, 8 * 4096);
        pt[0] = (uintptr_t)pt + 4096 + 3;
        for(i = 0; i < 5; i++) pt[512 + i] = (uintptr_t)pt + (i + 2) * 4096 + 3;
        for(i = 0; i < 6 * 512; i++) pt[1024 + i] = (uintptr_t)i * 2 * 1024 * 1024 + 0x83;
    }
    tags_buf = (uint8_t*)efi_alloc(TAGS_MAX);
    bio = NULL;
}

/**
 * Find the root partition
 */
int efi_findroot(void)
{
    uint8_t *buf = (uint8_t*)&fat, *end = (uint8_t*)&fat + sizeof(fat) - 0x2a - 4;
    uint64_t i, j, k, l, n, m, p;
    guid_t dppGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;
    guid_t bioGuid = EFI_BLOCK_IO_PROTOCOL_GUID;
    efi_hard_disk_device_path_t *hdd;
    efi_handle_t *handles = (efi_handle_t*)data;

    i = 512; memset(handles, 0, i);
    if(!EFI_ERROR(BS->LocateHandle(ByProtocol, &bioGuid, NULL, &i, handles))) {
        for(m = 0; m < 32; m++)
            if(handles[m] && !EFI_ERROR(BS->HandleProtocol(handles[m], &bioGuid, (void **)&bio)) && bio && bio->Media &&
              bio->Media->BlockSize == 512) {
                bio->ReadBlocks(bio, bio->Media->MediaId, 1, sizeof(fat), &buf);
                if(!memcmp(&buf, EFI_PTAB_HEADER_ID, 8)) {
                    j = ((gpt_header_t*)&buf)->SizeOfPartitionEntry;
                    l = ((gpt_header_t*)&buf)->PartitionEntryLBA;
                    n = ((gpt_header_t*)&buf)->NumberOfPartitionEntries;
                    for(k = 0, p = 1; n; k++) {
                        bio->ReadBlocks(bio, bio->Media->MediaId, l + k, sizeof(fat), &buf);
                        for(i = 0; n && i + j <= 512; i += j, n--, p++)
                            if(!memcmp(&((gpt_entry_t*)&buf[i])->UniquePartitionGUID, &rootuuid, sizeof(guid_t))) {
                                root_buf = (uint8_t*)efi_alloc(32); if(!root_buf) { bio = NULL; return 0; }
                                root_lba = ((gpt_entry_t*)&buf[i])->StartingLBA;
                                root_siz = ((gpt_entry_t*)&buf[i])->EndingLBA - root_lba + 1;
                                for(i = 0; i < 256; i++)
                                    bio->ReadBlocks(bio, bio->Media->MediaId, root_lba + i, 512, (void*)(root_buf + (i << 9)));
                                /* get a device path for this bio and append the root partition to it */
                                bootdev = NULL;
                                if(!EFI_ERROR(BS->HandleProtocol(handles[m], &dppGuid, (void **)&buf))) {
                                    memcpy(fat, buf, sizeof(fat));
                                    for(buf = (uint8_t*)&fat; buf < end && *buf != 0x7F && *buf != 4; buf += (buf[3] << 8) | buf[2]);
                                    if(buf < end) {
                                        /* replace end of path marker with root partition path */
                                        hdd = (efi_hard_disk_device_path_t*)buf;
                                        hdd->Type = 4;              /* Media Device Path */
                                        hdd->SubType = 1;           /* Hard Disk */
                                        hdd->Length[0] = 0x2a;
                                        hdd->Length[1] = 0;
                                        hdd->PartitionNumber = p;
                                        hdd->PartitionStart = root_lba;
                                        hdd->PartitionSize = root_siz;
                                        memcpy(&hdd->PartitionSignature, &rootuuid, sizeof(guid_t));
                                        hdd->PartitionFormat = 2;   /* GPT */
                                        hdd->SignatureFormat = 2;   /* GUID */
                                        /* append a new end of path marker */
                                        memcpy(buf + 0x2a, "\x7F\xFF\x04", 4);
                                        bootdev = (efi_device_path_t*)&fat;
                                    }
                                }
                                return 1;
                            }
                    } /* foreach sector */
                } /* has GPT */
            } /* if handle is a valid block device */
    }
    bio = NULL;
    return 0;
}

/**************** Common functions ****************/

/**
 * Allocate and zero out pages
 */
PLG_API void *alloc(uint32_t num)
{
    return (void*)(ST ? efi_alloc(num) : bios_alloc(num));
}

/**
 * Free allocated pages
 */
PLG_API void free(void *buf, uint32_t num)
{
    if(ST && BS && BS->FreePages) BS->FreePages((efi_physical_address_t)buf, num);
}

/**
 * Load a sector from root (or boot) partition
 */
PLG_API void loadsec(uint64_t sec, void *dst)
{
    if (fuzz_start)
    {
        if((((int32_t)sec + 1) * 512) <= payload_buffer->size)
        {
            memcpy(dst, payload_buffer->data + sec * 512, 512);
        }
        return;
    }
    
    sec += (root_lba ? root_lba : (ST ? 0 : (uint64_t)root_dir));
    if(ST) { if(bio) bio->ReadBlocks(bio, bio->Media->MediaId, sec, 512, dst); }
    else bios_loadsec(sec, dst);
}

/**
 * Set up file system hooks
 */
PLG_API void sethooks(void *o, void *r, void *c)
{
    plg_got.open = o;
    plg_got.read = r;
    plg_got.close = c;
}

/**
 * Initialize firmware related stuff
 */
void fw_init(efi_handle_t image, efi_system_table_t *systab)
{
    /* make sure SSE is enabled, because some say there are buggy firmware in the wild not enabling (and also needed if we come
     * from boot_x86.asm). No supported check, because according to AMD64 Spec Vol 2, all long mode capable CPUs must also
     * support SSE2 at least. We don't need them, but it's more than likely that a kernel is compiled using SSE instructions. */
    __asm__ __volatile__ (
    "movq %%cr0, %%rax;andb $0xF1, %%al;movq %%rax, %%cr0;"     /* clear MP, EM, TS (FPU emulation off) */
    "movq %%cr4, %%rax;orw $3 << 9, %%ax;movq %%rax, %%cr4;"    /* set OSFXSR, OSXMMEXCPT (enable SSE) */
    :::"rax");

    // /* initialize everything to zero */
    IM = image; ST = systab;
    root_dir = NULL; BS = NULL; bpb = NULL; f = NULL; memmap = NULL; initrd = NULL; num_memmap = verbose = edid_size = 0;
    conf_buf = kernel = cmdline = NULL; root_buf = krnl_buf = tags_ptr = rsdp_ptr = dsdt_ptr = edid_ptr = NULL;
    hack_buf = ram = root_lba = root_siz = krnl_size = 0; in_exc = rddir = smp = 0; memset(fn, 0, sizeof(fn));
    memset(&vidmode, 0, sizeof(vidmode)); pb_fb = NULL;

    // /* do firmware specific initialization */
    if(ST) efi_init(); else bios_init();
    if(!vidmode.framebuffer_addr) vidmode.framebuffer_width = vidmode.framebuffer_height = vidmode.framebuffer_bpp = 0;
    fb_fg = FB_COLOR(127, 127, 127); fb_bg = 0; pb_bg = FB_COLOR(32, 32, 32);
    console_init();
}

/**
 * Clear the screen
 */
void fw_clrscr(void)
{
    uint8_t *fb = (uint8_t*)vidmode.framebuffer_addr;
    int i, j, x, y, b = (vidmode.framebuffer_bpp + 7) >> 3;

    if(!fb) return;
    for(j = y = 0; y < (int)vidmode.framebuffer_height; y++, j += vidmode.framebuffer_pitch)
        for(i = j, x = 0; x < (int)vidmode.framebuffer_width; x++, i += b)
            if(b == 2) *((uint16_t*)(fb + i)) = (uint16_t)fb_bg; else *((uint32_t*)(fb + i)) = fb_bg;
#ifdef CONSOLE_FB
    fb_x = fb_y = 4;
#endif
}

/**
 * Display an image
 */
void fw_image(int x, int y, uint8_t *tga, uint32_t size)
{
    uint32_t c, R, G, B, A;
    uint8_t *fb = (uint8_t*)vidmode.framebuffer_addr;
    int i, j, k, l, w, h, o, m, p, px, py, b = (vidmode.framebuffer_bpp + 7) >> 3;

    /* only indexed RLE compressed TGA images supported */
    if(!fb || !tga || size < 18 || tga[0] || tga[1] != 1 || tga[2] != 9 || tga[3] || tga[4] || (tga[7] != 24 && tga[7] != 32)) return;
    /* uncompress image */
    o = (tga[17] & 0x20); w = (tga[13] << 8) + tga[12]; h = (tga[15] << 8) + tga[14];
    if(w < 1 || h < 1) return;
    px = x - w / 2; py = y - h / 2;
    m = ((tga[7] >> 3) * ((tga[6] << 8) | tga[5])) + 18; y = i = 0;
    if(tga[7] == 32) {
        R = ((fb_bg >> vidmode.framebuffer_red_field_position) & ~(0xffffffff << vidmode.framebuffer_red_mask_size)) <<
            (8 - vidmode.framebuffer_red_mask_size);
        G = ((fb_bg >> vidmode.framebuffer_green_field_position) & ~(0xffffffff << vidmode.framebuffer_green_mask_size)) <<
            (8 - vidmode.framebuffer_green_mask_size);
        B = ((fb_bg >> vidmode.framebuffer_blue_field_position) & ~(0xffffffff << vidmode.framebuffer_blue_mask_size)) <<
            (8 - vidmode.framebuffer_blue_mask_size);
        if(R < 8 && G < 8 && B < 8) R = G = B = 8;
    }
    for(l = 0, x = w, y = -1; l < w * h && (uint32_t)m < size;) {
        k = tga[m++];
        if(k > 127) { p = 0; k -= 127; j = tga[m++] * (tga[7] >> 3) + 18; } else { p = 1; k++; }
        l += k;
        while(k--) {
            if(p) j = tga[m++] * (tga[7] >> 3) + 18;
            if(x == w) { x = 0; i = ((py + (!o ? h - y - 1 : y)) * (int)vidmode.framebuffer_pitch + (px + x) * b); y++; }
            if(py + y > 0 && py + y < (int)vidmode.framebuffer_height - 1) {
                if(px + x > 0 && px + x < (int)vidmode.framebuffer_width - 1) {
                    if(tga[7] == 32) {
                        /* we have an alpha channel, do alpha blending */
                        A = 255 - tga[j + 3];
                        c = A > 252 ? fb_bg : (A < 3 ? FB_COLOR(tga[j + 2], tga[j + 1], tga[j + 0]) :
                            FB_COLOR(((R * A + tga[j + 2] * tga[j + 3]) >> 8), ((G * A + tga[j + 1] * tga[j + 3]) >> 8),
                            ((B * A + tga[j + 0] * tga[j + 3]) >> 8)));
                    } else {
                        /* no alpha channel, full black is transparent and that's it */
                        c = FB_COLOR(tga[j + 2], tga[j + 1], tga[j + 0]); if(!c) c = fb_bg;
                    }
                    if(b == 2) *((uint16_t*)(fb + i)) = (uint16_t)c; else *((uint32_t*)(fb + i)) = c;
                }
                i += b;
            }
            x++;
        }
    }
}

/**
 * Open a file
 */
PLG_API int fw_open(char *fn)
{
    uint16_t *d, c;
    char *s;

    file_size = 0;
    if(!fn || !*fn) return 0;
    /* UTF-8 to WCHAR */
    for(s = fn, d = wcname; *s && *s != ' ' && *s != '\r' && *s != '\n' && d < &wcname[PATH_MAX - 2]; d++) {
        c = utf8(&s); if(c == '\\' && *s == ' ') { c = ' '; s++; }
        *d = c;
    }
    *d = 0;
    return ST ? efi_open(wcname) : bios_open(wcname);
}

/**
 * Read data from file
 */
PLG_API uint64_t fw_read(uint64_t offs, uint64_t size, void *buf)
{
    if(!buf) return 0;
    *((uint64_t*)buf) = 0;
    if(!root_dir || offs >= file_size || !size) return 0;
    return ST ? efi_read(offs, size, buf) : bios_read(offs, size, buf);
}

/**
 * Close file
 */
PLG_API void fw_close(void)
{
    ST ? efi_close() : bios_close();
}

/**
 * Load list of available plugins
 */
void fw_loadplugins(void)
{
    plgids = (uint8_t*)0x40000;
    rddir = 1; memcpy(fn, "easyboot/", 10);
    if(!fw_open(fn)) { rddir = 0; return; }
    if(ST) efi_loadplugins(); else bios_close();
    rddir = 0;
}

/**
 * Find a plugin that can handle the data in buffer
 */
plg_hdr_t *fw_findplg(uint8_t type, uint8_t *buf)
{
    plg_hdr_t *hdr = NULL;
    plg_id_t *ids;
    plg_rel_t *rel;
    uint32_t i, offs = 0, o, n;
    uint8_t *plgs = plgids;
    uint64_t *addr, imm, m, t;

    if(!buf || !plgs) return NULL;

    /* iterate on plugins */
    for(; plgs < plgids + 0x40000 && *((uint16_t*)plgs); plgs += *((uint16_t*)plgs))
        if(plgs[10] == type) {
            ids = (plg_id_t*)(plgs + 12);
            if(type != PLG_T_TAG) {
                /* iterate on plugin ids */
                for(i = 0; i < plgs[11]; i++, ids++) {
                    o = ids->offs + offs;
                    switch(ids->type) {
                        case PLG_M_BYTE: o = o > 65535 ? 0 : buf[o]; break;
                        case PLG_M_WORD: o = o > 65534 ? 0 : *((uint16_t*)(buf + o)); break;
                        case PLG_M_DWORD: o = o > 65532 ? 0 : *((uint32_t*)(buf + o)); break;
                        case PLG_M_BADD: o = (o > 65535 ? 0 : buf[o]) + offs; break;
                        case PLG_M_WADD: o = (o > 65534 ? 0 : *((uint16_t*)(buf + o))) + offs; break;
                        case PLG_M_DADD: o = (o > 65532 ? 0 : *((uint32_t*)(buf + o))) + offs; break;
                    }
                    if(!ids->size) offs = o;
                    else if(ids->type == PLG_M_SRCH) {
                        for(o = offs; o + ids->size < 65536; o += (ids->offs ? ids->offs : 1))
                            if(!memcmp(buf + o, ids->magic, ids->size)) { offs = o; break; }
                        if(o + ids->size >= 65536) { i = 256; break; }
                    } else if(o + ids->size > 65535 || memcmp(buf + o, ids->magic, ids->size)) { i = 256; break; }
                }
                /* this is not the plugin we're looking for */
                if(i != plgs[11]) continue;
            }
            /* plugin found. If it doesn't have an address yet then we have to load and link it first */
            if(!*((uint64_t*)(plgs + 2))) {
                memcpy(fn, "easyboot/", 9); memcpy(fn + 9, ids, PATH_MAX - 9);
                /* this should never fail. We have already opened it successfully before */
                if(fw_open(fn)) {
                    hdr = (plg_hdr_t *)data;
                    fw_read(0, sizeof(plg_hdr_t), hdr);
                    *((uint64_t*)(plgs + 2)) = (uint64_t)alloc((hdr->memsz + 4095) >> 12);
                    hdr = (plg_hdr_t *)*((uint64_t*)(plgs + 2));
                    fw_read(0, file_size, hdr);
                    fw_close();
                    /* clear bss */
                    memset((uint8_t*)hdr + hdr->filesz, 0, hdr->memsz - hdr->filesz);
                    /* do relocations and linking */
                    hdr->entry += (uint32_t)(uintptr_t)hdr;
                    for(i = 0, rel = (plg_rel_t*)((uint8_t*)hdr + sizeof(plg_hdr_t) + hdr->nid * sizeof(plg_id_t));
                      i < hdr->nrel; i++, rel++) {
                        /* CLang is buggy and generates 32 bit shifts for 64 bit literals, hence the casting everywhere. */
                        imm = 0; addr = (uint64_t*)((uintptr_t)hdr + rel->offs); t = plg_imsk[PLG_R_IMSK(rel->type)];
                        /* get the immedate (addend + offset). */
                        for(m = 1, n = PLG_R_SBIT(rel->type); m && n <= PLG_R_EBIT(rel->type); m <<= 1)
                            if(t & m) {
                                if(*addr & m) imm |= (uint64_t)1UL << (uint64_t)n;
                                n++;
                            }
                        /* sign extend */
                        if(imm >> (n - 1)) imm |= (uint64_t)-1UL << (uint64_t)n;
                        /* add symbol's address */
                        imm += rel->type & PLG_A_GOTREL ? (uint64_t)&plg_got + ((PLG_R_SYM(rel->type) - 1) << 3) :
                            (!PLG_R_SYM(rel->type) ? (uint64_t)hdr : (uint64_t)((uint64_t*)&plg_got)[PLG_R_SYM(rel->type) - 1]);
                        if(rel->type & PLG_A_PCREL) imm -= (uint64_t)addr;
                        /* handle negate bit */
                        n = PLG_R_NBIT(rel->type);
                        if(n) {
                            if(imm >> 63UL) { imm = -imm; *addr |= (uint64_t)1UL << (uint64_t)n; }
                            else *addr &= ~((uint64_t)1UL << (uint64_t)n);
                        }
                        /* write back new immedate */
                        for(m = 1, n = PLG_R_SBIT(rel->type); m && n <= PLG_R_EBIT(rel->type); m <<= 1)
                            if(t & m) {
                                if(imm & ((uint64_t)1UL << (uint64_t)n)) *addr |= m;
                                else *addr &= ~m;
                                n++;
                            }
                    }
                }
            }
            /* for tag plugins, call the entry point, for everything else return the plugin's address */
            if(type == PLG_T_TAG) (*((void(* PLG_API)(void))((uintptr_t)((plg_hdr_t *)*((uint64_t*)(plgs + 2)))->entry)))();
            else return (plg_hdr_t *)*((uint64_t*)(plgs + 2));
        }
    return NULL;
}

/**
 * Read a key with millisec timeout
 * returns 0 on timeout, 1 - 9 if one of [1] - [9] keys pressed
 */
uint32_t fw_getkey(uint32_t timeout)
{
    efi_input_key_t key;
    uint32_t t = 0, k;

    /* progressbar is only shown if the total value is bigger than the screen width, so be sure, multiply by 10000 */
    pb_init(timeout * 10000);
    do {
        if(ST) {
            /* wait a bit, Stall argument is in usecs */
            BS->Stall(1000);
            /* check key on keyboard */
            key.UnicodeChar = 0;
            if(!EFI_ERROR(ST->ConIn->ReadKeyStroke(ST->ConIn, &key)) && key.UnicodeChar >= '1' && key.UnicodeChar <= '9')
                return key.UnicodeChar - '0';
        } else {
            bios_sleep();
            /* check key on PS/2 keyboard */
            __asm__ __volatile__ ("xorq %%rax, %%rax;inb $0x64, %%al;cmpb $0xff, %%al;je 2f;andb $1, %%al;jz 2f;"
            "inb $0x60, %%al;jmp 3f;2:xorb %%al, %%al;3:" : "=a"(k) : : "rbx");
            /* BIOSes are expected to enable scan code translation in PS/2 controller for compatibility, meaning
             * keyboard should use scancode set 1 during boot. [ESC] = 1, [1] = 2 ... [9] = 10 */
            if(k >= 2 && k <= 10) return k - 1;
        }
#ifdef CONSOLE_SERIAL
        /* check key on serial */
        __asm__ __volatile__ ("xorq %%rax, %%rax;movw $0x3fd, %%dx;inb %%dx, %%al;andb $1, %%al;jz 2f;"
        "subb $5, %%dl;inb %%dx, %%al;jmp 3f;2:xorb %%al,%%al;3:" : "=a"(k) : : "rbx", "rdx");
        if(k >= '1' && k <= '9') return k - '0';
        /* we might have received an escape sequence */
        if(k == 27) {
            /* CSI sequences also have numbers in them, but we must skip those */
            do {
                __asm__ __volatile__ ("xorq %%rax, %%rax;movw $0x3fd, %%dx;1:pause;inb %%dx, %%al;andb $1, %%al;jz 1b;"
                "subb $5, %%dl;inb %%dx, %%al" : "=a"(k) : : "rbx", "rdx");
            } while(k == '[' || k == ':' || k == ';' || k == ',' || (k >= '0' && k <= '9'));
        }
#endif
        pb_draw(t); t += 10000;
    } while(--timeout);
    return 0;
}

/**
 * Load a file from root (or boot partition)
 */
PLG_API uint8_t *fw_loadfile(char *path)
{
    plg_hdr_t *plg;
    uint8_t *d = NULL, *b;
    uint32_t siz;
    char *c = fn;

    if(!path || !*path) { file_size = 0; return NULL; }
    if(*path == '/') path++;
    for(; c < fn + sizeof(lfn) - 1 && *path && *path != ' ' && *path != '}' && *path != '\r' && *path != '\n'; path++) {
        if(*path == '\\' && path[1] == ' ') path++;
        *c++ = *path;
    }
    *c = 0;

    (void)plg;
    (void)b;
    (void)siz;
    if((*plg_got.open)(fn)) {

        if(verbose) printf("Loading '%s' (%ld bytes)...\n", fn, file_size);
        d = (uint8_t*)alloc((file_size + 4095) >> 12);
        if(d) {
            
            (*plg_got.read)(0, file_size, d);
            /* if it's gzip compressed */
            if((d[0] == 0x1f && d[1] == 0x8b) || (d[0] == 'G' && d[1] == 'U' && d[2] == 'D' && d[8] == 0x78)) {
                if(d[0] == 0x1f) memcpy(&siz, d + file_size - 4, 4);
                else siz = (((d[4] | (d[5] << 8)) + 7) & ~7) + ((d[6] | (d[7] << 8)) << 4);
                b = (uint8_t*)alloc((siz + 4095) >> 12);
                if(b) {
                    if(verbose) printf("Uncompressing (%ld bytes)...\n", siz);
                    uncompress(d, b, siz);
                    if(ST) { BS->FreePages((efi_physical_address_t)d, (file_size + 4095) >> 12); d = b; }
                    else { memcpy(d, b, siz); file_buf = (uint64_t)d + ((siz + 4095) & ~4095); }
                    file_size = siz;
                }
            } else
            /* if we have a plugin to decompress it */
            if((plg = fw_findplg(PLG_T_DECOMP, d)))
                d = (*((uint8_t*(* PLG_API)(uint8_t*))((uintptr_t)plg->entry)))(d);
        }
        (*plg_got.close)();
    } else
    if(verbose) printf("WARNING: unable to load '%s'\n", fn);
    return d;
}

/**
 * Get glyph width
 */
void glyphw(uint32_t chr, uint8_t *g)
{
    uint8_t *d;
    uint32_t i;
    for(d = (uint8_t*)font + font->characters_offs, i = 0; i < 0x110000; i++) {
        if(d[0] == 0xFF) { i += 65535; d++; }
        else if((d[0] & 0xC0) == 0xC0) { i += (((d[0] & 0x3F) << 8) | d[1]); d += 2; }
        else if((d[0] & 0xC0) == 0x80) { i += (d[0] & 0x3F); d++; }
        else { if(i == chr) { *g = d[4]; break; } d += 6 + d[1] * (d[0] & 0x40 ? 6 : 5); }
    }
}

/**
 * Load and parse config (everything except modules, also display the user menu)
 */
void fw_loadconfig(void)
{
#define GETCOMMAND  while(*s && (*s == '\r' || *s == '\n' || *s == ' ' || *s == '\t' || *s == '{' || *s == '}')) s++;\
                    for(a = s; *a && *a != ' ' && *a != '\r' && *a != '\n'; a++);\
                    for(e = a; *e && *e != '{' && *e != '}' && *e != '\r' && *e != '\n'; e++);\
                    for(; a < e && *a == ' '; a++);\
                    if(a >= e) { s = e; continue; }

    char *s, *e, *a;
    uint8_t *d;
    uint32_t r, g, b, w, h, p, l, defent, defto, nument;
#ifdef CONSOLE_FB
    int dx, dy;
    uint8_t ma = 0, gw;
    uint32_t siz;
#endif

    /* try to open configuration file */
    defent = nument = l = 0; defto = 60000; conf_ptr = kernel = cmdline = NULL; memset(&rootuuid, 0, sizeof(guid_t));
    if(fw_open("easyboot/menu.cfg")) {
        if(ST) {
            if(EFI_ERROR(BS->AllocatePool(EfiLoaderData, file_size + 1, (void**)&conf_buf)) || !conf_buf) { fw_close(); goto err; }
        } else {
            conf_buf = (char*)tags_buf;
            tags_buf += (file_size + 7) & ~7;
        }
        fw_read(0, file_size, conf_buf);
        conf_buf[file_size] = 0;
        fw_close();

#ifdef CONSOLE_FB
        /* when we have a config try to load a UNICODE font too, because we only have an ASCII one embedded */
        if((d = fw_loadfile("easyboot/font.sfn")) && !memcmp(d, "SFN2", 4) && (((ssfn_t*)d)->type & 15) == 3) {
            font = (ssfn_t*)d;
            siz = (file_size + 4095) >> 12;
        }
#endif
        /* we go three times: first parse top level commands and count menuentry lines */
        for(s = conf_buf; *s;) {
            GETCOMMAND;
            /* 's' points to the start of the command,
             * 'a' to the first argument,
             * 'e' to the end of the line */
            if(!memcmp(s, "verbose", 7)) { a = getint(a, &verbose); } else
            if(!memcmp(s, "framebuffer", 11)) {
                a = getint(a, &w); while(a < e && *a == ' ') a++;
                a = getint(a, &h); while(a < e && *a == ' ') a++;
                a = getint(a, &p); while(a < e && *a == ' ') a++;
                if(w >= 320 && w < 65536 && h >= 200 && h < 65536 && p >= 15 && p <= 32) {
                    r = vidmode.framebuffer_width; g = vidmode.framebuffer_height; b = vidmode.framebuffer_bpp;
                    if(ST) efi_gop(w, h, p); else bios_vbe(w, h, p);
                    if(!vidmode.framebuffer_addr) { if(ST) efi_gop(r, g, b); else bios_vbe(r, g, b); }
                    /* pixel format might have changed */
                    fb_fg = FB_COLOR(127, 127, 127); fb_bg = 0; pb_bg = FB_COLOR(32, 32, 32);
                }
                if(a < e && *a == '#') {
                    a++; a = gethex(a, &r, 2); a = gethex(a, &g, 2); a = gethex(a, &b, 2); while(a < e && *a == ' ') a++;
                    fb_fg = FB_COLOR(r, g, b);
                    if(a < e && *a == '#') {
                        a++; a = gethex(a, &r, 2); a = gethex(a, &g, 2 ); a = gethex(a, &b, 2); while(a < e && *a == ' ') a++;
                        fb_bg = FB_COLOR(r, g, b);
                        if(a < e && *a == '#') {
                            a++; a = gethex(a, &r, 2); a = gethex(a, &g, 2 ); a = gethex(a, &b, 2);
                            pb_bg = FB_COLOR(r, g, b);
                        }
                    }
                }
                fw_clrscr();
                cons = 2; printf((char*)copy); cons = 3;
            } else
            if(!memcmp(s, "default", 7)) { a = getint(a, &defent); while(a < e && *a == ' ') { a++; } a = getint(a, &defto); } else
            if(!memcmp(s, "menualign", 9)) {
#ifdef CONSOLE_FB
                if(*a == 'v') ma = 1;
#endif
            } else
            if(!memcmp(s, "menuentry", 9)) {
#ifdef CONSOLE_FB
                while(a < e && *a != ' ') a++;
                while(a < e && *a == ' ') a++;
                /* get label length in pixels */
                for(g = 0; a < e;) { p = utf8(&a); gw = 0; glyphw(p, &gw); g += gw; }
                if(g > l) l = g;
#endif
                nument++; if(nument == 9) break;
            }
            /* go to the next line */
            s = e;
        }
        if(defent < 1 || defent > nument) defent = defto = 1;
        if(nument > 1) {
            if(defto < 10 || defto > 60000) defto = 60000;
#ifdef CONSOLE_FB
            /* tricky, because if list does not fit on screen, we must cut it in half and display as two coloumns / two rows */
            if(ma) {
                /* vertical menu */
                w = (vidmode.framebuffer_width - l - 96 - font->width) / 2;
                if(vidmode.framebuffer_height < (nument + 1) * 64) {
                    w = vidmode.framebuffer_width / 2 - (l + 96 + font->width) - 32;
                    h = (vidmode.framebuffer_height - (nument + 1) / 2 * 64) / 2; dy = 1;
                } else {
                    h = (vidmode.framebuffer_height - nument * 64) / 2;
                    dy = 0;
                }
                if(w < 8) w = 8;
            } else {
                /* horizontal menu */
                w = vidmode.framebuffer_width / (nument + 1);
                if(w < l + 8) {
                    w = l + 8;
                    if(vidmode.framebuffer_width > w * ((nument + 1) / 2)) w = vidmode.framebuffer_width / ((nument + 1) / 2);
                    dx = (vidmode.framebuffer_width - w * ((nument + 1) / 2)) / 2;
                    dy = -64 - 2 * font->height;
                } else {
                    dx = (vidmode.framebuffer_width - w * nument) / 2;
                    dy = 0;
                }
                h = vidmode.framebuffer_height / 2;
            }
#endif
            /* second time display boot options */
            for(r = 1, s = conf_buf; *s;) {
                GETCOMMAND;
                if(r <= nument && !memcmp(s, "menuentry", 9)) {
#ifdef CONSOLE_FB
                    if(ma) {
                        if(dy && r - 1 == (nument + 1) / 2) {
                            w = vidmode.framebuffer_width / 2 + 32; h -= (r - 1) * 64;
                        }
                        b = (r - 1) * 64 + h + 32;
                        fb_x = w;
                        fb_y = b - font->height / 2;
                    } else {
                        if(dy && r - 1 == (nument + 1) / 2) {
                            dx = (vidmode.framebuffer_width - w * (nument - (nument + 1) / 2)) / 2 - w * ((nument + 1) / 2);
                            dy = -dy;
                        }
                        b = dx + w * r - w / 2;
                        fb_x = b;
                        fb_y = h + dy + 2 * font->height;
                    }
                    g = font->width; glyphw(r + '0', (uint8_t*)&g);
                    /* we must reuse buffers, we don't have the luxury of lots of memory */
                    memcpy(fn, "easyboot/", 9);
                    fw_image(fb_x, fb_y + 9, (uint8_t*)key_tga, sizeof(key_tga));
                    fb_x -= g / 2;
#endif
                    cons = 1; console_putc('[');
                    cons = 11; p = fb_fg; fb_fg = FB_COLOR(4,4,4); printf("%d", r); fb_fg = p;
                    cons = 1;
                    for(d = (uint8_t*)fn + 9; a < e && *a != ' '; a++, d++) *d = *a;
                    *d = 0; printf("] - %s ", fn + 9);
                    cons = 3;
#ifdef CONSOLE_FB
                    memcpy(d, ".tga", 5);
                    if(fw_open(fn)) {
                        fw_read(0, file_size, tags_buf);
                        if(ma) {
                            fb_x = w + 64;
                            fb_y = b;
                        } else {
                            fb_x = b;
                            fb_y = h + dy - 32;
                        }
                        fw_image(fb_x, fb_y, tags_buf, file_size);
                        fw_close();
                    } else if(verbose) printf("WARNING: unable to load '%S' ", wcname);
#endif
                    while(a < e && *a == ' ') a++;
#ifdef CONSOLE_FB
                    if(ma) {
                        fb_x = w + 96 + font->width;
                        fb_y = b - font->height / 2;
                    } else {
                        for(g = 0, s = a; s < e;) { p = utf8(&s); gw = 0; glyphw(p, &gw); g += gw; }
                        fb_x = b - g / 2;
                        fb_y = h + dy;
                    }
#endif
                    *e = 0; printf("%s", a); *e = '\n';
                    cons = 1; printf("\n"); cons = 3;
                    r++;
                }
                /* go to the next line */
                s = e;
            }
            /* now that the menu is shown, wait for user interaction with timeout */
            do {
                p = fw_getkey(defto);
                /* if we timed out, use the default entry */
                if(!p) p = defent;
            } while(p < 1 || p > nument);
        } else p = 1;
        if(verbose) { cons = 1; printf("Booting [%d]...\n", p); cons = 3; }

        /* third time parse only the selected menuentry's options */
        for(w = h = 0, s = conf_buf; *s && w <= p;) {
            GETCOMMAND;
            if(!memcmp(s, "menuentry", 9)) { w++; if(w == p) conf_ptr = e; } else
            if((w == p || nument < 2) && !memcmp(s, "bootsplash", 10)) {
                if(*a == '#') {
                    a++; a = gethex(a, &r, 2); a = gethex(a, &g, 2); a = gethex(a, &b, 2);
                    fb_bg = FB_COLOR(r, g, b);
                    while(a < e && *a != ' ') a++;
                    while(a < e && *a == ' ') a++;
                }
                fw_clrscr(); h = 1;
                if(a < e && fw_open(a)) {
                    fw_read(0, file_size, tags_buf);
                    fw_image(vidmode.framebuffer_width >> 1, vidmode.framebuffer_height >> 1, tags_buf, file_size);
                    fw_close();
                } else if(verbose) printf("WARNING: unable to load '%S'\n", wcname);
            } else
            if(w == p) {
                if(!memcmp(s, "partition", 9) && a + 35 <= e) { getguid(a, &rootuuid); } else
                if(!memcmp(s, "kernel", 6)) {
                    kernel = a;
                    for(; a < e && *a && *a != ' ' && *a != '}' && *a != '\r' && *a != '\n'; a++)
                        if(*a == '\\' && a[1] == ' ') a++;
                    while(a < e && *a == ' ') a++;
                    if(a < e && *a && *a != '}' && *a != '\r' && *a != '\n') {
                        cmdline = a;
                        /* look for root partition argument */
                        for(; a < e - 5; a++)
                            if(!memcmp(a, "root=", 5)) {
                                for(a += 5, s = a; s < e && *s && *s != ' ' && *s != '='; s++);
                                if(*s == '=') a = s + 1;
                                if(a + 35 <= e && a[8] == '-' && a[13] == '-' && a[18] == '-') { getguid(a, &rootuuid); }
                                break;
                            }
                    }
                } else
                if(!memcmp(s, "multicore", 9)) smp = 1;
            }
            /* go to the next line */
            s = e;
        }
#ifdef CONSOLE_FB
        /* restore built-in font */
        if(ST && font != (ssfn_t*)font_sfn) BS->FreePages((efi_physical_address_t)font, siz);
        font = (ssfn_t*)font_sfn;
#endif
        if(!h) fw_clrscr();
        if(verbose) { cons = 2; printf((char*)copy); cons = 3; }
    }
err:if(!kernel) kernel = (char*)defkernel;
}

/**
 * Detect config file independent configuration and generate tags for them
 */
void fw_loadsetup()
{
    multiboot_tag_loader_t *stag;
    multiboot_tag_mmap_t *mtag;
    char *c;

    if(!ST) file_buf = file_base;
    tags_ptr = tags_buf;
    if(tags_ptr) {
        /* MBI header */
        ((multiboot_info_t*)tags_buf)->total_size = ((multiboot_info_t*)tags_buf)->reserved = 0;
        tags_ptr += sizeof(multiboot_info_t);
        /* loader tag */
        stag = (multiboot_tag_loader_t*)tags_ptr;
        stag->type = MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME;
        stag->size = 17;
        memcpy(stag->string, EASYBOOT_MAGIC, 9);
        tags_ptr += 24;
        /* commandline tag */
        if(cmdline) {
            for(c = cmdline; *c && *c != '}' && *c != '\r' && *c != '\n'; c++);
            stag = (multiboot_tag_loader_t*)tags_ptr;
            stag->type = MULTIBOOT_TAG_TYPE_CMDLINE;
            stag->size = 9 + c - cmdline;
            memcpy(stag->string, cmdline, c - cmdline); stag->string[c - cmdline] = 0;
            tags_ptr += (stag->size + 7) & ~7;
            /* overwrite the cmdline pointer with this new, zero terminated string */
            cmdline = stag->string;
        }
        /* get memory map early, because we'll need it on BIOS to decide where to load the kernel
         * for UEFI this must be done as the last step, because the memory map will change during boot */
        if(!ST) {
            /* get system tables and generate tags for them */
            bios_systables();
            /* generate memory map tag */
            mtag = (multiboot_tag_mmap_t*)tags_ptr;
            mtag->type = MULTIBOOT_TAG_TYPE_MMAP;
            mtag->entry_size = sizeof(multiboot_mmap_entry_t);
            mtag->entry_version = 0;
            num_memmap = bios_e820(mtag->entries);
            if(num_memmap > 0) {
                memmap = mtag->entries;
                mtag->size = sizeof(multiboot_tag_mmap_t) + num_memmap * sizeof(multiboot_mmap_entry_t);
                tags_ptr += (mtag->size + 7) & ~7;
            }
        } else {
            /* get system tables and generate tags for them */
            efi_systables();
            /* we can't use it, however we still have to query the memory map on UEFI too to determine how much RAM we have */
            efi_memmap(NULL);
        }
        ram &= ~(2 * 1024 * 1024 - 1);
    }
}

/**
 * Parse config for modules and load them
 */
void fw_loadmodules(void)
{
    uint8_t *ptr;
    int n = 0, f = 0;
    multiboot_tag_module_t *tag;
    char *s, *e, *a;

    if(conf_ptr) {
        for(s = conf_ptr; !f && *s;) {
            /* find beginning of a line */
            while(*s && (*s == '\r' || *s == '\n' || *s == ' ' || *s == '\t')) s++;
            for(a = s; *a && *a != ' ' && *a != '\r' && *a != '\n'; a++);
            for(e = a; *e && *e != '\r' && *e != '\n'; e++);
            for(; a < e && *a == ' '; a++);
            /* 's' points to the start of the command,
             * 'a' to the first argument,
             * 'e' to the end of the line */
            if(a >= e) { s = e; continue; }
            if(!memcmp(s, "module", 6)) {
ldinitrd:       if((ptr = fw_loadfile(a))) {
                    /* if it's a DTB, DSDT or a GUDT, don't add it to the modules list, add it to the ACPI tables */
                    if(ptr[0] == 0xD0 && ptr[1] == 0x0D && ptr[2] == 0xFE && ptr[3] == 0xED) {
                        if(verbose) printf("DTB detected...\n");
                        dsdt_ptr = ptr;
                    } else
                    if(((ptr[0] == 'D' && ptr[1] == 'S') || (ptr[0] == 'G' && ptr[1] == 'U')) && ptr[2] == 'D' && ptr[3] == 'T') {
                        if(verbose) printf("%c%cDT detected...\n", ptr[0], ptr[1]);
                        dsdt_ptr = ptr;
                    } else {
                        if(tags_ptr) {
                            tag = (multiboot_tag_module_t*)tags_ptr;
                            tag->type = MULTIBOOT_TAG_TYPE_MODULE;
                            tag->size = sizeof(multiboot_tag_module_t) + e - a + 1;
                            tag->mod_start = (uint32_t)(uintptr_t)ptr;
                            tag->mod_end = (uint32_t)(uintptr_t)ptr + (uint32_t)file_size;
                            memcpy(tag->string, a, e - a); tag->string[e - a] = 0;
                            if(verbose > 2) printf("%D\n", ptr);
                            tags_ptr += (tag->size + 7) & ~7;
                            if(!initrd) initrd = tag;
                        }
                        n++;
                    }
                }
            }
            /* go to the next line */
            s = e;
        }
    }
    /* if no modules were loaded, but we have a default initrd name, try to add that */
    if(!n && !f && definitrd[0]) { f = 1; a = (char*)definitrd; for(e = a; *e; e++){} goto ldinitrd; }
}

/**
 * Map virtual memory
 */
int fw_map(uint64_t phys, uint64_t virt, uint32_t size)
{
    uint64_t end = virt + size, *ptr, *next = NULL, orig = file_buf;

    /* is this a canonical address? We handle virtual memory up to 256TB */
    if(!pt || ((virt >> 48L) != 0x0000 && (virt >> 48L) != 0xffff)) return 0;

    /* walk the page tables and add the missing pieces */
    for(virt &= ~4095, phys &= ~4095; virt < end; virt += 4096) {
        /* 512G */
        ptr = &pt[(virt >> 39L) & 511];
        if(!*ptr) { if(!(*ptr = (uint64_t)alloc(1))) return 0; else *ptr |= 3; }
        /* 1G */
        ptr = (uint64_t*)(*ptr & ~4095); ptr = &ptr[(virt >> 30L) & 511];
        if(!*ptr) { if(!(*ptr = (uint64_t)alloc(1))) return 0; else *ptr |= 3; }
        /* 2M if we previously had a large page here, split it into 4K pages */
        ptr = (uint64_t*)(*ptr & ~4095); ptr = &ptr[(virt >> 21L) & 511];
        if(!*ptr || *ptr & 0x80) { if(!(*ptr = (uint64_t)alloc(1))) return 0; else *ptr |= 3; }
        /* 4K */
        ptr = (uint64_t*)(*ptr & ~4095); ptr = &ptr[(virt >> 12L) & 511];
        /* if this page is already mapped, that means the kernel has invalid, overlapping segments */
        if(!*ptr) { *ptr = (uint64_t)next; next = ptr; }
    }
    /* resolve the linked list */
    for(end = ((phys == orig ? file_buf : phys) + size - 1) & ~4095; next; end -= 4096, next = ptr) {
        ptr = (uint64_t*)*next; *next = end | 3;
    }
    return 1;
}

/**
 * Load a kernel segment
 */
PLG_API int fw_loadseg(uint32_t offs, uint32_t filesz, uint64_t vaddr, uint32_t memsz)
{
    uint8_t *buf = (uint8_t*)(uintptr_t)vaddr;
    uint32_t size, i;

    if(verbose > 1) printf("  segment %08x[%08x] -> %08x[%08x]\n", offs, filesz, vaddr, memsz);
    if(!memsz || !krnl_size || offs + filesz > krnl_size) { printf("ERROR: out of bound segment\n"); return 1; }
    size = (memsz + (vaddr & 4095) + 4095) & ~4095;
    if(ST) {
        if(vaddr > ram) {
            /* possibly a higher-half kernel's segment, we must map it */
            if(!(buf = (uint8_t*)efi_alloc(size >> 12)) || !fw_map((uint64_t)buf, vaddr, size)) goto err;
        } else {
            /* if we have exited boot services already */
            if(!BS->AllocatePages) goto chk;
            /* try to allocate memory exactly at the requested address */
            vaddr &= ~4095;
            if(EFI_ERROR(BS->AllocatePages(AllocateAddress, EfiLoaderData, size >> 12, (efi_physical_address_t*)&vaddr)) || !vaddr)
                goto err;
            buf = (uint8_t*)(uintptr_t)vaddr;
        }
    } else {
        /* no overwriting of the loader data */
        if(vaddr < 0x20000 + (TAGS_MAX + 2) * 4096) goto err;
        if(vaddr > ram) {
            /* possibly a higher-half kernel's segment, we must map it */
            file_buf = (file_buf + 4095) & ~4095;
            if(!fw_map(file_buf, vaddr, size)) goto err;
            buf = (void*)file_buf; file_buf += size;
        } else {
            /* let's see if the memory where the segment wants to be loaded is free or not */
chk:        for(i = 0; memmap && i < num_memmap; i++)
                /* find which memory slot it fits */
                if(memmap[i].base_addr <= vaddr && memmap[i].base_addr + memmap[i].length > vaddr) {
                    /* if that slot isn't free or the end doesn't fit in the slot too, then that's a problem */
                    if(memmap[i].type != MULTIBOOT_MEMORY_AVAILABLE ||
                      memmap[i].base_addr + memmap[i].length < (vaddr & ~4095) + size)
                        goto err;
                    break;
                }
            /* if no memory slots found, that's a dunno. Not used memory for sure, so let's try to load it, maybe works... */
        }
    }
    if(filesz) memcpy(buf, krnl_buf + offs, filesz);
    if(memsz > filesz) memset(buf + filesz, 0, memsz - filesz);
    /* if we have updated the paging tables, kick the MMU */
    if(vaddr > ram)
        __asm__ __volatile__ ("movq %%cr3, %%rax;movq %%rax, %%cr3;" : : : "rax");

    return 1;
err:printf("ERROR: unable to load segment %08lx[%x], memory already in use\n", vaddr, memsz);
    return 0;
}

/**
 * Load the kernel
 */
int fw_loadkernel(void)
{
    void *p = (void*)krnl_buf;
    pe_hdr *pe;
    pe_sec *sec;
    uint8_t *ptr, *ent = NULL;
    uint64_t offs;
    int i;

    if(!memcmp(((Elf64_Ehdr*)p)->e_ident, ELFMAG, 4) && ((Elf64_Ehdr*)p)->e_ident[EI_CLASS] == ELFCLASS64) {
        /* it's a Multiboot2 ELF kernel */
        if(verbose) printf("Starting Multiboot2 ELF64 kernel...\n");
        if(((Elf64_Ehdr*)p)->e_machine != EM_X86_64) { bad: printf("ERROR: bad architecture\n"); goto err; }
        ent = (uint8_t*)(uintptr_t)((Elf64_Ehdr*)p)->e_entry;
        ptr = p + ((Elf64_Ehdr*)p)->e_phoff;
        for(i = 0; i < ((Elf64_Ehdr*)p)->e_phnum && ptr + ((Elf64_Ehdr*)p)->e_phentsize < krnl_buf + krnl_size;
          i++, ptr += ((Elf64_Ehdr*)p)->e_phentsize)
            if(((Elf64_Phdr*)ptr)->p_type == PT_LOAD && !fw_loadseg(
                (((Elf64_Phdr*)ptr)->p_offset), (((Elf64_Phdr*)ptr)->p_filesz),
                (((Elf64_Phdr*)ptr)->p_vaddr), (((Elf64_Phdr*)ptr)->p_memsz))) goto err;
    } else
    if(((mz_hdr*)p)->magic == MZ_MAGIC && ((pe_hdr*)(p + ((mz_hdr*)p)->peaddr))->magic == PE_MAGIC &&
       ((pe_hdr*)(p + ((mz_hdr*)p)->peaddr))->file_type == PE_OPT_MAGIC_PE32PLUS &&
       ((pe_hdr*)(p + ((mz_hdr*)p)->peaddr))->subsystem != IMAGE_SUBSYSTEM_EFI_APPLICATION) {
        /* it's a Multiboot2 COFF/PE kernel */
        pe = (pe_hdr*)(p + ((mz_hdr*)p)->peaddr);
        offs = pe->data.pe64.img_base;
        ent = offs + (uint8_t*)(uintptr_t)pe->entry_point;
        if(verbose) printf("Starting Multiboot2 PE32+ kernel...\n");
        if(((pe_hdr*)(p + ((mz_hdr*)p)->peaddr))->machine != IMAGE_FILE_MACHINE_AMD64) goto bad;
        sec = (pe_sec*)((uint8_t*)pe + pe->opt_hdr_size + 24);
        for(i = 0; i < pe->sections && (uint8_t*)&sec[1] < krnl_buf + krnl_size; i++, sec++)
            /* the PE section vaddr field is only 32 bits, we must make sure that it properly sign extended to 64 bit */
            if(!fw_loadseg(sec->raddr, sec->rsiz, offs + (int64_t)(int32_t)sec->vaddr, sec->vsiz)) goto err;
    } else {
        printf("ERROR: unknown kernel format\n");
        if(verbose > 2) printf("%4D", krnl_buf);
err:    return 0;
    }
    if(verbose > 2) printf("Kernel entry:\n%4D", ent);
    if(verbose > 1)
        printf("Transfering control to %08x(%08x, %08x[%x])\n", ent,
            MULTIBOOT2_BOOTLOADER_MAGIC, tags_buf, tags_ptr - tags_buf);
    /* tell APs to execute kernel */
    if(smp) { *((volatile uint64_t*)0x538) = (uint64_t)(uintptr_t)ent; __asm__ __volatile__("pause":::"memory"); }
    /* execute 64-bit kernels in long mode */
    __asm__ __volatile__(
    "movq %%rcx, %%r8;"
    /* SysV ABI uses %rdi, %rsi, but fastcall uses %rcx, %rdx */
    "movq %%rax, %%rcx;movq %%rax, %%rdi;"
    "movq %%rbx, %%rdx;movq %%rbx, %%rsi;"
    "movq $0x90000, %%rsp; movq %%rsp, %%rbp;"
    "jmp *%%r8"
    ::"a"(MULTIBOOT2_BOOTLOADER_MAGIC),"b"(tags_buf),"c"(ent):);
    return 1;
}

/**
 * Finish up MBI tags
 */
int fw_fini(plg_hdr_t *plg)
{
    int i;
    fadt_t *fadt;
    multiboot_tag_mmap_t *tag;
    uintn_t memory_map_size = 0, map_key = 0, desc_size = 0;
    uint8_t *rsdt, *ptr, *end, s;
    uint32_t a, b;
    uint64_t c;

    if(ST) {
        if(conf_buf) { BS->FreePool(conf_buf); conf_buf = NULL; }
        if(root_buf) { BS->FreePages((efi_physical_address_t)root_buf, 32); root_buf = NULL; }
        if(hack_buf) { BS->FreePages(hack_buf, 1024); hack_buf = 0; }
        if(plgids) {
            for(ptr = plgids; ptr < plgids + 0x40000 && *((uint16_t*)ptr); ptr += *((uint16_t*)ptr))
                if(*((uint64_t*)(ptr + 2)) && *((uint64_t*)(ptr + 2)) != (uint64_t)plg)
                    BS->FreePages(*((uint64_t*)(ptr + 2)), (((plg_hdr_t*)*((uint64_t*)(ptr + 2)))->memsz + 4095) >> 12);
            BS->FreePool(plgids); plgids = NULL;
        }
    }
    /* we must check this here, because if kernel type not detected and we have an EFI Application then we must not call
     * ExitBootServices and set up the environment here, otherwise calling StartImage later would be impossible */
    if(!plg && ((mz_hdr*)krnl_buf)->magic == MZ_MAGIC && ((pe_hdr*)(krnl_buf + ((mz_hdr*)krnl_buf)->peaddr))->magic == PE_MAGIC &&
      ((pe_hdr*)(krnl_buf + ((mz_hdr*)krnl_buf)->peaddr))->machine == IMAGE_FILE_MACHINE_AMD64 &&
      ((pe_hdr*)(krnl_buf + ((mz_hdr*)krnl_buf)->peaddr))->subsystem == IMAGE_SUBSYSTEM_EFI_APPLICATION) {
        if(ST) {
            /* free remaining buffers as well because not needed with an EFI Application */
            if(tags_buf) { BS->FreePages((efi_physical_address_t)tags_buf, TAGS_MAX); tags_buf = NULL; }
            BS->FreePages((efi_physical_address_t)pt, 8); pt = NULL;
        }
        return 0;
    }
    /* clear SMP semaphor */
    *((uint64_t*)0x538) = *((uint64_t*)0x522) = 0;
    /* measure CPU clock cycles */
    __asm__ __volatile__ ( "rdtsc" : "=a"(a),"=d"(b)); c = ((uint64_t)b << 32UL)|(uint64_t)a;
    ST ? BS->Stall(1000) : bios_sleep();
    __asm__ __volatile__ ( "rdtsc" : "=a"(a),"=d"(b));
    *((uint64_t*)0x548) = ((((uint64_t)b << 32UL)|(uint64_t)a) - c) / 5000;
    if(*((uint64_t*)0x548) < 1) *((uint64_t*)0x548) = 1;

    if(tags_ptr) {
        if(vidmode.framebuffer_addr) {
            vidmode.type = MULTIBOOT_TAG_TYPE_FRAMEBUFFER;
            vidmode.size = sizeof(vidmode);
            vidmode.framebuffer_type = 1;
            vidmode.reserved = 0;
            memcpy(tags_ptr, &vidmode, vidmode.size);
            tags_ptr += 40;
        }
        if(edid_ptr && edid_size > 0 && *((uint64_t*)(edid_ptr + 8))) {
            ((multiboot_tag_edid_t*)tags_ptr)->type = MULTIBOOT_TAG_TYPE_EDID;
            ((multiboot_tag_edid_t*)tags_ptr)->size = edid_size + 8;
            memcpy(tags_ptr + 8, edid_ptr, edid_size);
            tags_ptr += (edid_size + 15) & ~7;
        }
        /* this is problematic, we have to patch the DSDT address with the user provided table's */
        if(dsdt_ptr && rsdp_ptr) {
            rsdt = rsdp_ptr[15] >= 2 ? (uint8_t*)*((uint64_t*)(rsdp_ptr + 24)) : (uint8_t*)(uintptr_t)*((uint32_t*)(rsdp_ptr + 16));
            /* found RSDP, iterate on ACPI tables */
            if(rsdt && (rsdt[0] == 'R' || rsdt[0] == 'X') && !memcmp(rsdt + 1, "SDT", 3))
                for(ptr = rsdt + 36, end = (uint8_t*)(rsdt + ((fadt_t*)rsdt)->size); ptr < end;
                  ptr += rsdt[0] == 'X' ? 8 : 4) {
                    fadt = rsdt[0] == 'X' ? (fadt_t*)((uintptr_t)*((uint64_t*)ptr)) :
                        (fadt_t*)((uintptr_t)*((uint32_t*)ptr));
                    /* found FADT, patch DSDT addresses and recalculate checksum */
                    if(!memcmp(&fadt->magic, "FACP", 4)) {
                        fadt->dsdt = (uint32_t)(uintptr_t)dsdt_ptr;
                        if(fadt->rev >= 2 && fadt->size > sizeof(fadt_t)) fadt->x_dsdt = (uintptr_t)dsdt_ptr;
                        fadt->chksum = 0;
                        for(s = 0, i = 0; i < (int)fadt->size; i++) { s += *(((uint8_t*)fadt) + i); }
                        fadt->chksum = 0x100 - s;
                        break;
                    }
                }
        }
        if(ST) {
            tag = (multiboot_tag_mmap_t*)tags_ptr;
            tag->type = MULTIBOOT_TAG_TYPE_MMAP;
            tag->entry_size = sizeof(multiboot_mmap_entry_t);
            tag->entry_version = 0;
            num_memmap = efi_memmap(tag->entries);
            if(num_memmap > 0) {
                memmap = tag->entries;
                tag->size = sizeof(multiboot_tag_mmap_t) + num_memmap * sizeof(multiboot_mmap_entry_t);
                tags_ptr += (tag->size + 7) & ~7;
            }
            ((multiboot_tag_efi64_t*)tags_ptr)->type = MULTIBOOT_TAG_TYPE_EFI64;
            ((multiboot_tag_efi64_t*)tags_ptr)->size = 16;
            ((multiboot_tag_efi64_t*)tags_ptr)->pointer = (uintptr_t)ST;
            tags_ptr += 16;
            ((multiboot_tag_efi64_ih_t*)tags_ptr)->type = MULTIBOOT_TAG_TYPE_EFI64_IH;
            ((multiboot_tag_efi64_ih_t*)tags_ptr)->size = 16;
            ((multiboot_tag_efi64_ih_t*)tags_ptr)->pointer = (uintptr_t)IM;
            tags_ptr += 16;
        }
        if(smp) {
            ((multiboot_tag_smp_t*)tags_ptr)->type = MULTIBOOT_TAG_TYPE_SMP;
            ((multiboot_tag_smp_t*)tags_ptr)->size = 24;
            ((multiboot_tag_smp_t*)tags_ptr)->numcores = ((multiboot_tag_smp_t*)tags_ptr)->running = 1;
            ((multiboot_tag_smp_t*)tags_ptr)->bspid = 0;
            tags_ptr += 24;
        }
        /* call tag plugins to add optional tags to the Multiboot2 tags list */
        *((uint32_t*)data) = MULTIBOOT2_BOOTLOADER_MAGIC;
        fw_findplg(PLG_T_TAG, data);
        /* terminator tag */
        ((multiboot_tag_t*)tags_ptr)->type = MULTIBOOT_TAG_TYPE_END;
        ((multiboot_tag_t*)tags_ptr)->size = 8;
        tags_ptr += 8;
        ((multiboot_info_t*)tags_buf)->total_size = tags_ptr - tags_buf;
    } else tags_buf = NULL;
    if(ST) {
        BS->GetMemoryMap(&memory_map_size, NULL, &map_key, &desc_size, NULL);
        BS->ExitBootServices(IM, map_key);
    }
    /* now that we have left the firmware realm behind, we can get some real work done :-) */
    *((uint64_t*)0x530) = (uintptr_t)pt;
    *((uint64_t*)0x540) = (uintptr_t)tags_buf;
    __asm__ __volatile__ (
    /* on UEFI, we haven't set our paging tables yet, because we can only do this after we have called ExitBootServices */
    "movq %%rax, %%cr3;"
    /* Set up dummy exception handlers */
    ".byte 0xe8;.long 0;"                       /* absolute address to set the code segment register */
    "1:popq %%rax;"
    "movq %%rax, %%rsi;addq $4f - 1b, %%rsi;"   /* pointer to the code stubs */
    "movq %%rax, %%rdi;addq $5f - 1b, %%rdi;"   /* pointer to IDT */
    "addq $3f - 1b, %%rax;movw $0x40, (0x510);movq %%rax, (0x512);"
    "lgdt (0x510);"                             /* we must set up a new GDT with a TSS */
    "addq $64, %%rax;"                          /* patch GDT and load TR */
    "movq %%rax, %%rcx;andl $0xffffff, %%ecx;addl %%ecx, -14(%%rax);"
    "movq %%rax, %%rcx;shrq $24, %%rcx;movq %%rcx, -9(%%rax);"
    "movq $48, %%rax;ltr %%ax;"
    "movw $32, %%cx;\n"                         /* we set up 32 entires in IDT */
    "1:movq %%rsi, %%rax;movw $0x8F01, %%ax;shlq $16, %%rax;movw $32, %%ax;shlq $16, %%rax;movw %%si, %%ax;stosq;"
    "movq %%rsi, %%rax;shrq $32, %%rax;stosq;"
    "addq $16, %%rsi;decw %%cx;jnz 1b;"         /* next entry */
    "movw $2f-5f-1,(0x520);cmpq $0, (0x522);jnz 1f;movq $5f, (0x522);1:"
    "lidt (0x520);jmp 2f;"                      /* set up IDT */
    /* new GDT */
    ".balign 8;3:;"
    ".quad 0;"                                  /* null descriptor */
    ".long 0x0000FFFF;.long 0x00009800;"        /*   8 - legacy real cs */
    ".long 0x0000FFFF;.long 0x00CF9A00;"        /*  16 - prot mode cs */
    ".long 0x0000FFFF;.long 0x00CF9200;"        /*  24 - prot mode ds */
    ".long 0x0000FFFF;.long 0x00AF9A00;"        /*  32 - long mode cs */
    ".long 0x0000FFFF;.long 0x00CF9200;"        /*  40 - long mode ds */
    ".long 0x00000068;.long 0x00008900;"        /*  48 - long mode tss descriptor */
    ".long 0x00000000;.long 0x00000000;"        /*       cont. */
    /* TSS */
    ".long 0;.long 0x1000;.long 0;.long 0x1000;.long 0;.long 0x1000;.long 0;"
    ".long 0;.long 0;.long 0x1000;.long 0;"
    /* ISRs */
    "1:popq %%r8;movq 16(%%rsp),%%r9;jmp fw_exc;"
    ".balign 16;4:xorq %%rdx, %%rdx; xorb %%cl, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $1, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $2, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $3, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $4, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $5, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $6, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $7, %%cl;jmp 1b;"
    ".balign 16;popq %%rdx; movb $8, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $9, %%cl;jmp 1b;"
    ".balign 16;popq %%rdx; movb $10, %%cl;jmp 1b;"
    ".balign 16;popq %%rdx; movb $11, %%cl;jmp 1b;"
    ".balign 16;popq %%rdx; movb $12, %%cl;jmp 1b;"
    ".balign 16;popq %%rdx; movb $13, %%cl;jmp 1b;"
    ".balign 16;popq %%rdx; movb $14, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $15, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $16, %%cl;jmp 1b;"
    ".balign 16;popq %%rdx; movb $17, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $18, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $19, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $20, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $21, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $22, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $23, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $24, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $25, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $26, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $27, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $28, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $29, %%cl;jmp 1b;"
    ".balign 16;popq %%rdx; movb $30, %%cl;jmp 1b;"
    ".balign 16;xorq %%rdx, %%rdx; movb $31, %%cl;jmp 1b;"
    /* IDT */
    ".balign 16;5:.space (32*16);2:"
    ::"a"(pt):"rcx","rsi","rdi");
    return 1;
}

/**
 * Dummy exception handler
 */
void fw_exc(uint8_t excno, uint64_t exccode, uint64_t rip, uint64_t rsp)
{
    uint64_t cr2, cr3;
#if defined(CONSOLE_FB) || defined(CONSOLE_VGA)
    uint32_t i;
#endif
#ifdef CONSOLE_FB
    uint32_t j, x, y, b;
#endif
    if(!in_exc) {
        in_exc++;
        __asm__ __volatile__("movq %%cr2, %%rax;movq %%cr3, %%rbx;":"=a"(cr2),"=b"(cr3)::);
#ifdef CONSOLE_FB
        if(vidmode.framebuffer_addr) {
            b = (vidmode.framebuffer_bpp + 7) >> 3;
            fb_x = fb_y = 4; fb_fg = FB_COLOR(255, 255, 255); fb_bg = FB_COLOR(255, 0, 0);
            for(j = y = 0; y < (8 + ((uint32_t)font->height << 1)); y++, j += vidmode.framebuffer_pitch)
                for(i = j, x = 0; x < vidmode.framebuffer_width; x++, i += b)
                    if(b == 2) *((uint16_t*)(vidmode.framebuffer_addr + i)) = (uint16_t)fb_bg;
                    else *((uint32_t*)(vidmode.framebuffer_addr + i)) = fb_bg;
        }
#endif
#ifdef CONSOLE_VGA
        if(!vidmode.framebuffer_addr && !ST)
            for(i = vga_x = vga_y = 0; i < 320; i += 2) *((uint16_t*)((uintptr_t)0xB8000 + i)) = 0x4f20;
#endif
        printf("Easyboot Exception Handler\nException %02x #%s code %016x\n\n",
            excno, excno < 32 ? excstr[(int)excno] : "INTR", exccode);
#ifdef CONSOLE_FB
        fb_bg = 0;
#endif
#ifdef CONSOLE_VGA
        if(!vidmode.framebuffer_addr && !ST)
            for(i = 0; i < 320; i += 2) *((uint16_t*)((uintptr_t)0xB8000 + i)) |= 0x4f00;
#endif
        printf("RIP %016x RSP %016x CR2 %016x CR3 %016x\n\nCode\n%D\nStack\n%4D\n",
            rip, rsp, cr2, cr3, rip, rsp);
    }
    __asm__ __volatile__("1: cli; hlt; jmp 1b");
}

/*****************************************
 *      Easyboot loader entry point      *
 *****************************************/




efi_status_t _start (efi_handle_t image, efi_system_table_t *systab)
{
    efi_memory_mapped_device_path_t mdev[2];
    efi_status_t status;
    efi_handle_t image_handle;
    plg_hdr_t *plg;

    /* initialize UEFI or BIOS */
    fw_init(image, systab);
    printf((char*)copy);

    
    /* now that we can display error messages, let's see if we got everything we need */
    if(!pt || !tags_buf) { printf("ERROR: unable to allocate memory\n"); goto err; }
    if(!root_dir) { printf("ERROR: boot partition not found\n"); goto err; }

    /* load configuration */
    fw_loadconfig();
    fw_loadsetup();
    if(ram < 62*1024*1024) { printf("ERROR: unknown amount of RAM\n"); goto err; }
    else if(verbose) printf("Physical RAM %ld Megabytes\n", ram / 1024 / 1024 + 2);

    /* load plugins and look for root partition */
    fw_loadplugins();
    
    
    init_fuzz_intc();

    volatile host_config_t host_config;
    volatile agent_config_t agent_config = {0};

    kAFL_hypercall(HYPERCALL_KAFL_ACQUIRE, 0);
    kAFL_hypercall(HYPERCALL_KAFL_RELEASE, 0);

    kAFL_hypercall(HYPERCALL_KAFL_USER_SUBMIT_MODE, KAFL_MODE_64);


    kAFL_hypercall(HYPERCALL_KAFL_GET_HOST_CONFIG, (uint64_t)&host_config);
    if (host_config.host_magic != NYX_HOST_MAGIC || host_config.host_version != NYX_HOST_VERSION) 
    {
        habort("magic error\n");
    }
    agent_config.agent_magic = NYX_AGENT_MAGIC;
    agent_config.agent_version = NYX_AGENT_VERSION;

    agent_config.agent_tracing = 0;
    agent_config.agent_ijon_tracing = 0;
    agent_config.agent_non_reload_mode = 0; // persistent fuzz!
    agent_config.coverage_bitmap_size = host_config.bitmap_size;

    payload_buffer = alloc(host_config.payload_buffer_size / 4096 + 1);
    kAFL_hypercall (HYPERCALL_KAFL_SET_AGENT_CONFIG, (uint64_t)&agent_config);

    uint64_t buffer_kernel[3] = {0};


    buffer_kernel[0] = 0x7000;
    buffer_kernel[1] = 0x20000000;
    buffer_kernel[2] = 0;
    kAFL_hypercall(HYPERCALL_KAFL_RANGE_SUBMIT, (uint64_t)buffer_kernel);
    kAFL_hypercall (HYPERCALL_KAFL_GET_PAYLOAD, (uint64_t)payload_buffer);
    
    kAFL_hypercall (HYPERCALL_KAFL_NEXT_PAYLOAD, 0);
    kAFL_hypercall (HYPERCALL_KAFL_ACQUIRE, 0);

    
    fuzz_start = 1;
    if(!(plg = fw_findplg(PLG_T_FS, payload_buffer->data))) 
    {
        printf("ERROR: no plugin to parse root partition\n");
    }
    else
    {
        
        root_buf = payload_buffer->data;
        (*((void(* PLG_API)(void))((uintptr_t)plg->entry)))();
        fw_loadfile("/a.txt");
        fw_loadfile("/a.txt.ln");
       
    }
    


    kAFL_hypercall (HYPERCALL_KAFL_RELEASE, 0);

    while (1)
    {
        ;
    }
    





    if(rootuuid.Data1 || rootuuid.Data2 || rootuuid.Data3) {
        if(!(ST ? efi_findroot() : bios_findroot())) { printf("ERROR: root partition not found\n"); goto err; }
        if(!(plg = fw_findplg(PLG_T_FS, root_buf)) && !(plg = fw_findplg(PLG_T_FS, root_buf + 65536))) {
            printf("ERROR: no plugin to parse root partition\n"); goto err;
        }
        /* call the file system plugin's entry point to initialize and set its hooks */
        (*((void(* PLG_API)(void))((uintptr_t)plg->entry)))();
    }






    /* now we have the root partition as well as the kernel's filename, try to load that, it's a critical error if fails */
    if(!(krnl_buf = fw_loadfile(kernel)) || !file_size) { printf("ERROR: kernel not found\n"); goto err; }
    krnl_size = file_size;
    /* detect kernel's type */
    plg = fw_findplg(PLG_T_KERNEL, krnl_buf);

    /* finally load modules too */
    fw_loadmodules();

    /* finish up things, finalize tags list (and look for EFI Application kernel, because then we must omit ExitBootServices) */
    if(!fw_fini(plg)) {
        if(verbose) printf("Starting EFI Application...\n");
        if(ST) {
            /* we have an EFI Application */
            if(!bootdev) {
                /* from the UEFI spec section 7.4.1:
                 * "If SourceBuffer is not NULL, the function is a memory-to-memory load in which SourceBuffer points to the
                 * image to be loaded and SourceSize indicates the image’s size in bytes. In this case, the caller has copied
                 * the image into SourceBuffer and can free the buffer once loading is complete. The DevicePath is optional in
                 * this case."
                 * in reality buggy UEFI implementations demand DevicePath to be set, even when there's a SourceBuffer */
                memset(mdev, 0, sizeof(mdev));
                /* EFI device path */
                mdev[0].Type = 1;       /* Hardware Device Path */
                mdev[0].SubType = 3;    /* Memory Mapped Path */
                mdev[0].Length[0] = 24;
                mdev[0].MemoryType = EfiLoaderData;
                mdev[0].StartAddress = (uint64_t)krnl_buf;
                mdev[0].EndAddress = (uint64_t)krnl_buf + krnl_size;
                mdev[1].Type = 0x7F;    /* End Device Path */
                mdev[1].SubType = 0xFF; /* End Entire Device Path */
                mdev[1].Length[0] = 4;
                bootdev = (efi_device_path_t *)mdev;
            }
            status = BS->LoadImage(0, image, bootdev, (void*)krnl_buf, krnl_size, &image_handle);
            BS->FreePages((efi_physical_address_t)krnl_buf, (krnl_size + 4095) >> 12);
            if(!EFI_ERROR(status)) status = BS->StartImage(image_handle, NULL, NULL);
        } else status = EFI_UNSUPPORTED;
        if(EFI_ERROR(status)) printf("ERROR: EFI StartImage failed %lx\n", status);
        goto err;
    }

    /* transfer control to kernel. Should never return */
    if(verbose) printf("Parsing kernel...\n");
    if(plg) (*((void(* PLG_API)(uint8_t *, uint64_t))((uintptr_t)plg->entry)))(krnl_buf, krnl_size);
    else fw_loadkernel();
    printf("ERROR: unable to start kernel\n");

    /* there's nowhere to return to on BIOS, halt machine */
err:if(!systab) {
        __asm__ __volatile__("1: cli; hlt; jmp 1b");
    }
    /* on UEFI we should return an error status, and the firmware should get the next option from the boot order list */
    return EFI_LOAD_ERROR;
}
