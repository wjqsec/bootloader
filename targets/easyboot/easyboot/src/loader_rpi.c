/*
 * src/loader_rpi.c
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
 * @brief The main Easyboot loader program on Raspberry Pi
 *
 * Memory layout when booted on Raspberry Pi:
 *      0x0 -    0x100  reserved by firmware (armstub)
 *    0x508 -   0x510   sctlr
 *    0x510 -   0x518   vbar
 *    0x518 -   0x520   mair
 *    0x520 -   0x528   tcr
 *    0x528 -   0x530   ttbr0
 *    0x530 -   0x538   ttbr1
 *    0x538 -   0x540   kernel entry point (also SMP semaphor)
 *    0x540 -   0x548   tags_buf
 *    0x100 -   0x1000  stack (3840 bytes)
 *   0x1000 -   0x9000  paging tables
 *   0x9000 - _bss_end  our sections
 *  0x20000 -  0x40000  config + tags
 *  0x40000 -  0x80000  firmware provided FDT (dtb); from the top to bottom: kernel's stack
 *  0x80000 - 0x100000  plugin ids
 * 0x100000 -        x  kernel segments, followed by the modules, each page aligned
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
#define CONSOLE_SERIAL                              /* default serial, UART0 */
#define CONSOLE_FB                                  /* on screen too */

/* it is VERY important that these two variables must be the first in the
 * read-only data segment, because the disk generator might alter them */
const char __attribute__((aligned(16))) defkernel[64] = "kernel", definitrd[64] = "";
/* patched by bin2h.c, must be right after the default names. Must use volatile otherwise Clang optimizes memread away */
const volatile unsigned int _bss_start = 0xffffffff, _bss_end = 0xffffffff;
const char copy[] = "Easyboot loader, Copyright (c) 2023 bzt, GPLv3+\n";

/**
 * Assembly preambule to C
 * must be the very first function; it can't use any local variables and can't have a stack frame
 */
void __attribute__((noreturn)) /*__attribute__((naked))*/ _preambule(void)
{
    /* make sure we run on BSP only, set up EL1 and a stack */
    __asm__ __volatile__(
    /* move FDT out of the way (up to 256k. As of now the biggest RPi-400 dtb is 54k, overlays typically ca. 4k, biggest 46k) */
    "mov x1,#0x40000; mov w3,#32768;1:ldr x2,[x0],#8; str x2,[x1],#8; sub w3,w3,#1; cbnz w3,1b;"
    /* relocate ourselves from 0x80000 to 0x9000 */
    "mov x0,#0x80000; mov x1,#0x9000; mov w3,#16384;1:ldr x2,[x0],#8; str x2,[x1],#8; sub w3,w3,#1; cbnz w3,1b;"
    "ldr x30,=1f;ret;.balign 64;.asciz \"Easyboot https://gitlab.com/bztsrc/easyboot\";.asciz \"aarch64\";.balign 4;1:"
    /* setup */
    "mrs x1, mpidr_el1;and x1, x1, #3;cbz x1, 2f;"  /* get core id and branch if BSP */
    "1:wfe; b 1b;"                                  /* neverending loop for APs */
    "2:mov x1, #0x1000;"
    "mrs x0, CurrentEL;and x0, x0, #12;"
    "cmp x0, #12;bne 1f;"                           /* are we running at EL3? */
    "mov x0, #0x5b1;msr scr_el3, x0;mov x0, #0x3c9;msr spsr_el3, x0;adr x0, 1f;msr elr_el3, x0;mov x0, #4;msr sp_el2, x1;eret;"
    "1:cmp x0, #4;beq 1f;"                          /* are we running at EL2? */
    "mrs x0,cnthctl_el2;orr x0,x0,#3;msr cnthctl_el2,x0;msr cnthp_ctl_el2,xzr;"         /* enable CNTP */
    "mov x0,#(1 << 31);orr x0,x0,#2;msr hcr_el2,x0;mrs x0,hcr_el2;"                     /* enable Aarch64 at EL1 */
    "mrs x0,midr_el1;mrs x2,mpidr_el1;msr vpidr_el2,x0;msr vmpidr_el2,x2;"              /* initialize virtual MPIDR */
    "mov x0,#0x33FF;msr cptr_el2,x0;msr hstr_el2,xzr;mov x0,#(3<<20);msr cpacr_el1,x0;" /* disable coprocessor traps */
    "mov x2,#0x0800;movk x2,#0x30d0,lsl #16;msr sctlr_el1, x2;"                         /* setup SCTLR access */
    "mov x2,#0x3c5;msr spsr_el2,x2;adr x2, 1f;msr elr_el2, x2;mov sp, x1;msr sp_el1, x1;eret;"/* switch to EL1 */
    "1:adr x0,1f;msr vbar_el1,x0;mov x2,#0x0510;str x0, [x2], #0;msr SPSel,#0;"         /* set up exception handlers */
    "mov sp, x1;b _start;"                        /* set up stack and jump to C function in EL1 */
    /* exception handlers */
    ".balign 128;1:mov x0,#0;mrs x1,esr_el1;mrs x2,elr_el1;mrs x3,spsr_el1;mrs x4,far_el1;mrs x5,sctlr_el1;mrs x6,tcr_el1;b fw_exc;"
    ".balign 128;mov x0,#1;mrs x1,esr_el1;mrs x2,elr_el1;mrs x3,spsr_el1;mrs x4,far_el1;mrs x5,sctlr_el1;mrs x6,tcr_el1;b fw_exc;"
    ".balign 128;mov x0,#2;mrs x1,esr_el1;mrs x2,elr_el1;mrs x3,spsr_el1;mrs x4,far_el1;mrs x5,sctlr_el1;mrs x6,tcr_el1;b fw_exc;"
    ".balign 128;mov x0,#3;mrs x1,esr_el1;mrs x2,elr_el1;mrs x3,spsr_el1;mrs x4,far_el1;mrs x5,sctlr_el1;mrs x6,tcr_el1;b fw_exc;"
    :::"x0","x1","x2");
    /* naked not supported, so noreturn and unreachable needed to make the compiler omit the stack frame prologue / epilogue */
    __builtin_unreachable();
}

#include "loader.h"

/* IMPORTANT: don't assume .bss is zeroed out like in a hosted environment, because it's not */
volatile uint32_t  __attribute__((aligned(16))) mbox[40];
uint64_t mmio_base, emmc_base;
uint64_t file_size, file_base, file_buf, ram, *pt;
uint32_t fb_fg, fb_bg, pb_bg, pb_b, pb_m, pb_l, verbose, num_memmap;
uint8_t rpi, cons, *tags_buf, *tags_ptr, *dtb_base, *plgids, *root_buf, *krnl_buf, *pb_fb, in_exc, rddir, smp;
uint64_t root_dir, data_lba, fat_lba, root_lba, root_siz, krnl_size;
uint8_t vbr[512], data[512];
uint32_t fat[1024], fat_cache, file_clu;
uint16_t lfn[272], wcname[PATH_MAX];
efi_system_table_t *ST;
guid_t rootuuid;
esp_bpb_t *bpb;
multiboot_tag_framebuffer_t vidmode;
multiboot_tag_module_t *initrd;
char fn[PATH_MAX], *conf_buf, *conf_ptr, *kernel, *cmdline;
/* first one is common, others are architecture dependent */
uint64_t plg_imsk[] = { PLG_IM_0, PLG_IM_AARCH64_1, PLG_IM_AARCH64_2, PLG_IM_AARCH64_3 };
plg_got_t plg_got = {
    &verbose, &file_size, &root_buf, &tags_buf, &tags_ptr, &dtb_base, &dtb_base, &ST,
    &memset, &memcpy, &memcmp, &alloc, &free,
    &printf, &pb_init, &pb_draw, &pb_fini,
    &loadsec, &sethooks,
    &fw_open, &fw_read, &fw_close, &fw_loadfile,
    &fw_loadseg
};

typedef struct {
    char magic[8];  /* "RSD PTR " */
    uint8_t chksum;
    char OEM[6];
    uint8_t rev;    /* 2 */
    uint32_t rsdt;
} __attribute__((packed)) rsdp_t;

typedef struct {
    char magic[4];
    uint32_t size;
    uint8_t rev;
    uint8_t chksum;
    char OEM[6];
    char OEMtableid[8];
    uint32_t OEMrev;
    uint32_t creatid;
    uint32_t creatrev;
} __attribute__((packed)) sdt_hdr_t;

typedef struct {
    sdt_hdr_t hdr;
    uint32_t table_ptr[2];
} __attribute__((packed)) __attribute__((aligned(16))) rsdt_t;

typedef struct {
    uint8_t type;   /* 0 processor */
    uint8_t size;   /* 8 */
    uint8_t acpi_id;
    uint8_t apic_id;
    uint32_t flags; /* bit 0: enabled, bit 1: available */
} __attribute__((packed)) cpu_entry_t;

typedef struct {
    sdt_hdr_t hdr;  /* magic "APIC" */
    uint32_t lapic_addr;
    uint32_t flags;
    cpu_entry_t cpus[4];
} __attribute__((packed)) __attribute__((aligned(16))) apic_t;

typedef struct {
    sdt_hdr_t hdr;  /* magic "FACP" */
    uint32_t firmwarectrl;
    uint32_t dsdt;
    uint8_t  reserved[96];
    uint64_t x_dsdt;
} __attribute__((packed)) __attribute__((aligned(16))) fadt_t;

apic_t apic;
rsdt_t rsdt;
rsdp_t rsdp;
fadt_t fadt;

/**************** Mandatory functions, Clang generates calls to them ****************/

PLG_API void memcpy(void *dst, const void *src, uint32_t n){uint8_t *a=(uint8_t*)dst,*b=(uint8_t*)src;while(n--) *a++=*b++; }
PLG_API void memset(void *dst, uint8_t c, uint32_t n){uint8_t *a=dst;while(n--) *a++=c; }
PLG_API int  memcmp(const void *s1, const void *s2, uint32_t n){
    uint8_t *a=(uint8_t*)s1,*b=(uint8_t*)s2;while(n--){if(*a!=*b){return *a-*b;}a++;b++;} return 0;
}

#include "inflate.h"

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

/**************** GPU MailBox interface ****************/

#define VIDEOCORE_MBOX  (mmio_base+0x0000B880)
#define MBOX_READ       ((volatile uint32_t*)(VIDEOCORE_MBOX+0x0))
#define MBOX_POLL       ((volatile uint32_t*)(VIDEOCORE_MBOX+0x10))
#define MBOX_SENDER     ((volatile uint32_t*)(VIDEOCORE_MBOX+0x14))
#define MBOX_STATUS     ((volatile uint32_t*)(VIDEOCORE_MBOX+0x18))
#define MBOX_CONFIG     ((volatile uint32_t*)(VIDEOCORE_MBOX+0x1C))
#define MBOX_WRITE      ((volatile uint32_t*)(VIDEOCORE_MBOX+0x20))
#define MBOX_REQUEST    0
#define MBOX_RESPONSE   0x80000000
#define MBOX_FULL       0x80000000
#define MBOX_EMPTY      0x40000000
#define MBOX_CH_POWER   0
#define MBOX_CH_FB      1
#define MBOX_CH_VUART   2
#define MBOX_CH_VCHIQ   3
#define MBOX_CH_LEDS    4
#define MBOX_CH_BTNS    5
#define MBOX_CH_TOUCH   6
#define MBOX_CH_COUNT   7
#define MBOX_CH_PROP    8

uint8_t mbox_call(uint8_t ch)
{
    uint32_t r;

    /* mailbox write */
    do{ __asm__ __volatile__("nop"); } while(*MBOX_STATUS & MBOX_FULL);
    *MBOX_WRITE = (((uint32_t)((uint64_t)mbox) & ~0xF) | (ch & 0xF));
    /* mailbox read */
    do {
        do{ __asm__ __volatile__("nop");} while(*MBOX_STATUS & MBOX_EMPTY);
        r = *MBOX_READ;
    } while((uint8_t)(r & 0xF) != ch);
    return (r & ~0xF) == (uint32_t)((uint64_t)mbox) && mbox[1] == MBOX_RESPONSE;
}

/**
 * Set up framebuffer with VideoCore
 */
void mbox_lfb(uint32_t width, uint32_t height, uint32_t bpp)
{
    /* query natural width, height if not given */
    if(!width || !height) {
        mbox[0] = 12*4;
        mbox[1] = MBOX_REQUEST;
        mbox[2] = 0x40003;  /* get phy wh */
        mbox[3] = 8;
        mbox[4] = 8;
        mbox[5] = 0;
        mbox[6] = 0;
        mbox[7] = 0x40005;  /* get depth */
        mbox[8] = 4;
        mbox[9] = 4;
        mbox[10] = 0;
        mbox[11] = 0;
        if(mbox_call(MBOX_CH_PROP) && mbox[5] && mbox[10]) {
            width = mbox[5];
            height = mbox[6];
            if(width < 800) width = 800;
            if(height < 600) height = 600;
            bpp = 32;
        }
    }
    /* if we already have a framebuffer, release it */
    if(vidmode.framebuffer_addr) {
        mbox[0] = 8*4;
        mbox[1] = MBOX_REQUEST;
        mbox[2] = 0x48001;  /* release buffer */
        mbox[3] = 8;
        mbox[4] = 8;
        mbox[5] = (uint32_t)vidmode.framebuffer_addr;
        mbox[6] = 0;
        mbox[7] = 0;
        mbox_call(MBOX_CH_PROP);
        vidmode.framebuffer_addr = 0;
    }
    /* check minimum resolution */
    if(width < 320) width = 320;
    if(height < 200) height = 200;
    if(bpp != 15 && bpp != 16 && bpp != 24) bpp = 32;

    mbox[0] = 35*4;
    mbox[1] = MBOX_REQUEST;

    mbox[2] = 0x48003;  /* set phy wh */
    mbox[3] = 8;
    mbox[4] = 8;
    mbox[5] = width;
    mbox[6] = height;

    mbox[7] = 0x48004;  /* set virt wh */
    mbox[8] = 8;
    mbox[9] = 8;
    mbox[10] = width;
    mbox[11] = height;

    mbox[12] = 0x48009; /* set virt offset */
    mbox[13] = 8;
    mbox[14] = 8;
    mbox[15] = 0;
    mbox[16] = 0;

    mbox[17] = 0x48005; /* set depth */
    mbox[18] = 4;
    mbox[19] = 4;
    mbox[20] = bpp;

    mbox[21] = 0x48006; /* set pixel order */
    mbox[22] = 4;
    mbox[23] = 4;
    mbox[24] = 0;       /* BGR, not RGB preferably */

    mbox[25] = 0x40001; /* get framebuffer, gets alignment on request */
    mbox[26] = 8;
    mbox[27] = 8;
    mbox[28] = 4096;
    mbox[29] = 0;

    mbox[30] = 0x40008; /* get pitch */
    mbox[31] = 4;
    mbox[32] = 4;
    mbox[33] = 0;

    mbox[34] = 0;

    if(mbox_call(MBOX_CH_PROP) && mbox[20] == bpp && mbox[27] == (MBOX_RESPONSE|8) && mbox[28]) {
        vidmode.framebuffer_addr = (uint64_t)(mbox[28] & 0x3FFFFFFF);
        vidmode.framebuffer_pitch = mbox[33];
        vidmode.framebuffer_width = mbox[5];
        vidmode.framebuffer_height = mbox[6];
        vidmode.framebuffer_bpp = mbox[20];
        vidmode.framebuffer_type = 1;
        if(mbox[24]) {
            /* red is the least significant channel */
            switch(mbox[20]) {
                case 15:
                    vidmode.framebuffer_red_mask_size = vidmode.framebuffer_green_mask_size =
                        vidmode.framebuffer_blue_mask_size = 5;
                    vidmode.framebuffer_red_field_position = 0;
                    vidmode.framebuffer_green_field_position = 5;
                    vidmode.framebuffer_blue_field_position = 10;
                break;
                case 16:
                    vidmode.framebuffer_red_mask_size = vidmode.framebuffer_blue_mask_size = 5;
                    vidmode.framebuffer_green_mask_size = 6;
                    vidmode.framebuffer_red_field_position = 0;
                    vidmode.framebuffer_green_field_position = 5;
                    vidmode.framebuffer_blue_field_position = 11;
                break;
                default:
                    vidmode.framebuffer_red_mask_size = vidmode.framebuffer_green_mask_size =
                        vidmode.framebuffer_blue_mask_size = 8;
                    vidmode.framebuffer_red_field_position = 0;
                    vidmode.framebuffer_green_field_position = 8;
                    vidmode.framebuffer_blue_field_position = 16;
                break;
            }
        } else {
            /* blue is the least significant channel */
            switch(mbox[20]) {
                case 15:
                    vidmode.framebuffer_red_mask_size = vidmode.framebuffer_green_mask_size =
                        vidmode.framebuffer_blue_mask_size = 5;
                    vidmode.framebuffer_red_field_position = 10;
                    vidmode.framebuffer_green_field_position = 5;
                    vidmode.framebuffer_blue_field_position = 0;
                break;
                case 16:
                    vidmode.framebuffer_red_mask_size = vidmode.framebuffer_blue_mask_size = 5;
                    vidmode.framebuffer_green_mask_size = 6;
                    vidmode.framebuffer_red_field_position = 11;
                    vidmode.framebuffer_green_field_position = 5;
                    vidmode.framebuffer_blue_field_position = 0;
                break;
                default:
                    vidmode.framebuffer_red_mask_size = vidmode.framebuffer_green_mask_size =
                        vidmode.framebuffer_blue_mask_size = 8;
                    vidmode.framebuffer_red_field_position = 16;
                    vidmode.framebuffer_green_field_position = 8;
                    vidmode.framebuffer_blue_field_position = 0;
                break;
            }
        }
    } else {
        if(vidmode.framebuffer_addr) {
            mbox[0] = 8*4;
            mbox[1] = MBOX_REQUEST;
            mbox[2] = 0x48001;  /* release buffer */
            mbox[3] = 8;
            mbox[4] = 8;
            mbox[5] = (uint32_t)vidmode.framebuffer_addr;
            mbox[6] = 0;
            mbox[7] = 0;
            mbox_call(MBOX_CH_PROP);
            vidmode.framebuffer_addr = 0;
        }
        vidmode.framebuffer_width = vidmode.framebuffer_height = vidmode.framebuffer_bpp = 0;
    }
}

/**************** Early boot console ****************/

#define GPFSEL0         ((volatile uint32_t*)(mmio_base+0x00200000))
#define GPFSEL1         ((volatile uint32_t*)(mmio_base+0x00200004))
#define GPFSEL2         ((volatile uint32_t*)(mmio_base+0x00200008))
#define GPFSEL3         ((volatile uint32_t*)(mmio_base+0x0020000C))
#define GPFSEL4         ((volatile uint32_t*)(mmio_base+0x00200010))
#define GPFSEL5         ((volatile uint32_t*)(mmio_base+0x00200014))
#define GPSET0          ((volatile uint32_t*)(mmio_base+0x0020001C))
#define GPSET1          ((volatile uint32_t*)(mmio_base+0x00200020))
#define GPCLR0          ((volatile uint32_t*)(mmio_base+0x00200028))
#define GPLEV0          ((volatile uint32_t*)(mmio_base+0x00200034))
#define GPLEV1          ((volatile uint32_t*)(mmio_base+0x00200038))
#define GPEDS0          ((volatile uint32_t*)(mmio_base+0x00200040))
#define GPEDS1          ((volatile uint32_t*)(mmio_base+0x00200044))
#define GPHEN0          ((volatile uint32_t*)(mmio_base+0x00200064))
#define GPHEN1          ((volatile uint32_t*)(mmio_base+0x00200068))
#define GPPUD           ((volatile uint32_t*)(mmio_base+0x00200094))
#define GPPUDCLK0       ((volatile uint32_t*)(mmio_base+0x00200098))
#define GPPUDCLK1       ((volatile uint32_t*)(mmio_base+0x0020009C))

#define UART0_DR        ((volatile uint32_t*)(mmio_base+0x00201000))
#define UART0_FR        ((volatile uint32_t*)(mmio_base+0x00201018))
#define UART0_IBRD      ((volatile uint32_t*)(mmio_base+0x00201024))
#define UART0_FBRD      ((volatile uint32_t*)(mmio_base+0x00201028))
#define UART0_LCRH      ((volatile uint32_t*)(mmio_base+0x0020102C))
#define UART0_CR        ((volatile uint32_t*)(mmio_base+0x00201030))
#define UART0_IMSC      ((volatile uint32_t*)(mmio_base+0x00201038))
#define UART0_ICR       ((volatile uint32_t*)(mmio_base+0x00201044))

#ifdef CONSOLE_FB
uint8_t key_tga[645] = { 0,1,9,0,0,35,0,24,0,0,0,0,24,0,23,0,8,32,0,0,0,92,95,100,95,96,98,101,99,103,100,100,106,103,104,106,107,108,113,111,113,118,116,116,121,125,125,131,131,133,139,139,140,147,142,144,148,146,150,153,152,156,162,158,163,168,161,166,170,169,173,178,175,178,184,179,183,189,185,190,194,189,196,199,196,200,204,200,205,208,205,210,213,212,217,220,217,222,225,221,226,229,224,227,228,224,228,231,226,231,234,227,232,235,232,238,242,234,240,243,0,0,0,6,0,28,25,24,22,21,19,139,18,4,19,21,23,28,0,11,28,27,29,26,25,24,22,21,20,19,18,18,133,19,5,20,22,24,25,29,23,3,25,26,27,30,131,27,1,25,25,130,24,0,23,130,22,6,21,21,24,27,30,30,23,23,23,25,25,29,33,32,31,30,27,27,26,25,25,24,24,23,22,22,21,21,31,29,27,26,23,22,23,24,27,32,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,30,27,26,25,20,19,21,22,26,32,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,29,130,25,23,17,19,20,25,33,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,27,25,24,24,23,16,17,18,24,33,32,31,30,27,27,26,25,25,24,24,23,22,22,21,21,27,25,24,23,23,14,16,17,24,33,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,26,24,23,22,23,13,14,15,23,33,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,25,23,22,22,23,13,13,14,22,32,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,25,22,21,22,130,13,20,22,32,32,31,30,27,27,26,25,25,24,24,23,22,22,21,20,25,21,21,22,130,13,20,22,32,32,31,30,30,27,26,25,25,24,24,23,22,21,21,20,25,22,21,22,20,13,14,13,22,32,32,31,30,27,27,26,25,25,24,24,22,22,21,21,20,25,130,22,23,13,13,11,21,32,32,31,30,27,27,26,25,25,24,24,22,22,21,21,20,23,20,22,22,23,12,11,10,21,32,32,31,30,27,26,26,25,25,24,24,22,22,21,21,20,22,17,20,22,16,10,10,9,17,32,32,31,30,27,26,26,25,25,24,24,22,22,130,21,3,19,14,17,19,23,9,9,7,6,25,33,32,31,31,30,30,27,27,26,25,24,24,23,24,23,9,11,14,17,1,8,7,130,4,2,10,11,12,134,13,8,12,11,10,9,4,6,9,11,14,0,7,130,4,3,6,7,8,9,131,10,131,9,7,8,7,6,4,4,6,9,10,11,3,4,4,6,7,8,9,9,10,9,10,10,132,9,6,8,7,6,4,4,6,3,4,2,4,6,7,8,140,9,5,8,8,6,4,1,2,1,0,2,147,5,1,2,0 };
uint8_t font_sfn[2174] = { 83,70,78,50,126,8,0,0,0,0,8,16,11,13,38,0,15,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,1,24,24,128,0,126,128,9,91,64,1,65,64,1,65,64,1,109,128,6,24,60,60,60,24,24,24,128,3,102,102,102,36,128,8,54,54,127,54,54,54,127,54,54,128,13,24,24,62,99,67,3,62,96,96,97,99,62,24,24,128,7,67,99,48,24,12,6,99,97,128,9,28,54,54,28,110,59,51,51,51,110,128,3,12,12,12,4,128,9,48,24,12,12,12,12,12,12,24,48,128,9,12,24,48,48,48,48,48,48,24,12,128,4,102,60,255,60,102,128,4,24,24,126,24,24,128,3,24,24,24,12,128,0,127,128,7,64,96,48,24,12,6,3,1,128,9,28,54,99,99,107,107,99,99,54,28,128,9,24,28,30,24,24,24,24,24,24,126,128,9,62,99,96,48,24,12,6,3,99,127,128,9,62,99,96,96,60,96,96,96,99,62,128,9,48,56,60,54,51,127,48,48,48,120,128,9,127,3,3,3,63,96,96,96,99,62,128,9,28,6,3,3,63,99,99,99,99,62,128,9,127,99,96,96,48,24,12,12,12,12,128,9,62,99,99,99,62,99,99,99,99,62,128,9,62,99,99,99,126,96,96,96,48,30,128,2,24,24,12,128,8,96,48,24,12,6,12,24,48,96,128,8,6,12,24,48,96,48,24,12,6,128,6,62,99,99,48,24,24,24,128,8,62,99,99,123,123,123,59,3,62,128,9,8,28,54,99,99,127,99,99,99,99,128,9,63,102,102,102,62,102,102,102,102,63,128,9,60,102,67,3,3,3,3,67,102,60,128,9,31,54,102,102,102,102,102,102,54,31,128,9,127,102,70,22,30,22,6,70,102,127,128,9,127,102,70,22,30,22,6,6,6,15,128,9,60,102,67,3,3,123,99,99,102,92,128,9,99,99,99,99,127,99,99,99,99,99,128,9,60,24,24,24,24,24,24,24,24,60,128,9,120,48,48,48,48,48,51,51,51,30,128,9,103,102,102,54,30,30,54,102,102,103,128,9,15,6,6,6,6,6,6,70,102,127,128,9,99,119,127,127,107,99,99,99,99,99,128,9,99,103,111,127,123,115,99,99,99,99,128,9,62,99,99,99,99,99,99,99,99,62,128,9,63,102,102,102,62,6,6,6,6,15,128,11,62,99,99,99,99,99,99,107,123,62,48,112,128,9,63,102,102,102,62,54,102,102,102,103,128,9,62,99,99,6,28,48,96,99,99,62,128,9,126,126,90,24,24,24,24,24,24,60,128,9,99,99,99,99,99,99,99,99,99,62,128,9,99,99,99,99,99,99,99,54,28,8,128,9,99,99,99,99,107,107,107,127,119,54,128,9,99,99,54,62,28,28,62,54,99,99,128,9,102,102,102,102,60,24,24,24,24,60,128,9,127,99,97,48,24,12,6,67,99,127,128,9,60,12,12,12,12,12,12,12,12,60,128,8,1,3,7,14,28,56,112,96,64,128,9,60,48,48,48,48,48,48,48,48,60,128,3,8,28,54,99,128,0,255,128,2,12,12,24,128,6,30,48,62,51,51,51,110,128,9,7,6,6,30,54,102,102,102,102,62,128,6,62,99,3,3,3,99,62,128,9,56,48,48,60,54,51,51,51,51,110,128,6,62,99,127,3,3,99,62,128,9,28,54,38,6,15,6,6,6,6,15,128,9,110,51,51,51,51,51,62,48,51,30,128,9,7,6,6,54,110,102,102,102,102,103,128,6,28,24,24,24,24,24,60,128,1,96,96,128,9,112,96,96,96,96,96,96,102,102,60,128,9,7,6,6,102,54,30,30,54,102,103,128,9,28,24,24,24,24,24,24,24,24,60,128,6,55,127,107,107,107,107,99,128,6,59,102,102,102,102,102,102,128,6,62,99,99,99,99,99,62,128,9,59,102,102,102,102,102,62,6,6,15,128,9,110,51,51,51,51,51,62,48,48,120,128,6,59,110,102,6,6,6,15,128,6,62,99,6,28,48,99,62,128,9,8,12,12,63,12,12,12,12,108,56,128,6,51,51,51,51,51,51,110,128,6,102,102,102,102,102,60,24,128,6,99,99,107,107,107,127,54,128,6,99,54,28,28,28,54,99,128,9,99,99,99,99,99,99,126,96,48,31,128,6,127,51,24,12,6,99,127,128,9,112,24,24,24,14,24,24,24,24,112,128,11,24,24,24,24,24,24,24,24,24,24,24,24,128,9,14,24,24,24,112,24,24,24,24,14,128,1,110,59,128,6,8,28,54,99,99,99,127,0,1,8,16,9,0,0,2,45,0,0,158,0,0,8,16,9,0,0,2,8,16,9,0,0,2,57,0,0,0,10,38,0,0,0,1,8,16,9,0,0,1,66,0,0,0,1,8,16,9,0,0,3,72,0,0,0,1,8,16,9,0,0,0,83,0,0,0,1,8,16,9,0,0,4,99,0,0,0,1,8,16,9,0,0,2,109,0,0,0,1,8,16,9,0,0,1,121,0,0,0,1,8,16,9,0,0,2,127,0,0,0,1,8,16,9,0,0,2,139,0,0,0,1,8,16,9,0,0,5,151,0,0,0,1,8,16,9,0,0,5,158,0,0,0,1,8,16,9,0,0,9,165,0,0,0,1,8,16,9,0,0,7,171,0,0,0,1,8,16,9,0,0,10,38,0,0,0,1,8,16,9,0,0,4,174,0,0,0,1,8,16,9,0,0,2,184,0,0,0,1,8,16,9,0,0,2,196,0,0,0,1,8,16,9,0,0,2,208,0,0,0,1,8,16,9,0,0,2,220,0,0,0,1,8,16,9,0,0,2,232,0,0,0,1,8,16,9,0,0,2,244,0,0,0,1,8,16,9,0,0,2,0,1,0,0,1,8,16,9,0,0,2,12,1,0,0,1,8,16,9,0,0,2,24,1,0,0,1,8,16,9,0,0,2,36,1,0,0,2,8,16,9,0,0,4,38,0,0,0,9,38,0,0,0,2,8,16,9,0,0,4,38,0,0,0,9,48,1,0,0,1,8,16,9,0,0,3,53,1,0,0,2,8,16,9,0,0,5,42,0,0,0,8,42,0,0,0,1,8,16,9,0,0,3,64,1,0,0,2,8,16,9,0,0,2,75,1,0,0,10,38,0,0,0,1,8,16,9,0,0,3,84,1,0,0,1,8,16,9,0,0,2,95,1,0,0,1,8,16,9,0,0,2,107,1,0,0,1,8,16,9,0,0,2,119,1,0,0,1,8,16,9,0,0,2,131,1,0,0,1,8,16,9,0,0,2,143,1,0,0,1,8,16,9,0,0,2,155,1,0,0,1,8,16,9,0,0,2,167,1,0,0,1,8,16,9,0,0,2,179,1,0,0,1,8,16,9,0,0,2,191,1,0,0,1,8,16,9,0,0,2,203,1,0,0,1,8,16,9,0,0,2,215,1,0,0,1,8,16,9,0,0,2,227,1,0,0,1,8,16,9,0,0,2,239,1,0,0,1,8,16,9,0,0,2,251,1,0,0,1,8,16,9,0,0,2,7,2,0,0,1,8,16,9,0,0,2,19,2,0,0,1,8,16,9,0,0,2,31,2,0,0,1,8,16,9,0,0,2,45,2,0,0,1,8,16,9,0,0,2,57,2,0,0,1,8,16,9,0,0,2,69,2,0,0,1,8,16,9,0,0,2,81,2,0,0,1,8,16,9,0,0,2,93,2,0,0,1,8,16,9,0,0,2,105,2,0,0,1,8,16,9,0,0,2,117,2,0,0,1,8,16,9,0,0,2,129,2,0,0,1,8,16,9,0,0,2,141,2,0,0,1,8,16,9,0,0,2,153,2,0,0,1,8,16,9,0,0,3,165,2,0,0,1,8,16,9,0,0,2,176,2,0,0,1,8,16,9,0,0,0,188,2,0,0,1,8,16,9,0,0,13,194,2,0,0,1,8,16,9,0,0,0,197,2,0,0,1,8,16,9,0,0,5,202,2,0,0,1,8,16,9,0,0,2,211,2,0,0,1,8,16,9,0,0,5,223,2,0,0,1,8,16,9,0,0,2,232,2,0,0,1,8,16,9,0,0,5,244,2,0,0,1,8,16,9,0,0,2,253,2,0,0,1,8,16,9,0,0,5,9,3,0,0,1,8,16,9,0,0,2,21,3,0,0,2,8,16,9,0,0,2,38,0,0,0,5,33,3,0,0,2,8,16,9,0,0,2,42,3,0,0,5,46,3,0,0,1,8,16,9,0,0,2,58,3,0,0,1,8,16,9,0,0,2,70,3,0,0,1,8,16,9,0,0,5,82,3,0,0,1,8,16,9,0,0,5,91,3,0,0,1,8,16,9,0,0,5,100,3,0,0,1,8,16,9,0,0,5,109,3,0,0,1,8,16,9,0,0,5,121,3,0,0,1,8,16,9,0,0,5,133,3,0,0,1,8,16,9,0,0,5,142,3,0,0,1,8,16,9,0,0,2,151,3,0,0,1,8,16,9,0,0,5,163,3,0,0,1,8,16,9,0,0,5,172,3,0,0,1,8,16,9,0,0,5,181,3,0,0,1,8,16,9,0,0,5,190,3,0,0,1,8,16,9,0,0,5,199,3,0,0,1,8,16,9,0,0,5,211,3,0,0,1,8,16,9,0,0,2,220,3,0,0,1,8,16,9,0,0,2,232,3,0,0,1,8,16,9,0,0,2,246,3,0,0,1,8,16,9,0,0,2,2,4,0,0,1,8,16,9,0,0,4,6,4,0,159,0,0,0,0,9,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,254,255,254,255,254,255,254,255,195,95,50,78,70,83 };
typedef struct { uint8_t magic[4]; uint32_t size; uint8_t type; uint8_t features; uint8_t width; uint8_t height; uint8_t baseline; uint8_t underline; uint16_t fragments_offs; uint32_t characters_offs; uint32_t ligature_offs; uint32_t kerning_offs; uint32_t cmap_offs; } __attribute__((packed)) ssfn_t;
ssfn_t *font = (ssfn_t*)font_sfn;
uint32_t fb_x, fb_y;
#endif

/**
 * Initialize the console
 */
void console_init(void)
{
#ifdef CONSOLE_SERIAL
    uint32_t r;
#endif
    /* initialize UART */
    *UART0_CR = 0;          /* turn off UART0 */
#ifdef CONSOLE_SERIAL
    /* set up clock for consistent divisor values */
    mbox[0] = 8*4;
    mbox[1] = MBOX_REQUEST;
    mbox[2] = 0x38002;      /* set clock rate */
    mbox[3] = 12;
    mbox[4] = 8;
    mbox[5] = 2;            /* UART clock */
    mbox[6] = 4000000;      /* 4Mhz */
    mbox[7] = 0;            /* set turbo */
    mbox_call(MBOX_CH_PROP);
    r = *GPFSEL1;
    r &= ~((7<<12)|(7<<15));/* gpio14, gpio15 */
    r |= (4<<12)|(4<<15);   /* alt0 */
    *GPFSEL1 = r;
    *GPPUD = 0;             /* enable pins 14 and 15 */
    for(r = 0; r < 150; r++) __asm__ __volatile__("nop");
    *GPPUDCLK0 = (1<<14)|(1<<15);
    for(r = 0; r < 150; r++) __asm__ __volatile__("nop");
    *GPPUDCLK0 = 0;         /* flush GPIO setup */
    *UART0_ICR = 0x7FF;     /* clear interrupts */
    *UART0_IBRD = 2;        /* 115200 baud */
    *UART0_FBRD = 0xB;
    *UART0_LCRH = 0x03<<5;  /* 8n1 */
    *UART0_CR = 0x301;      /* enable Tx, Rx, FIFO */
#endif
#ifdef CONSOLE_FB
    fb_x = fb_y = 4;
#endif
    cons = 3;
}

/**
 * Display a character on boot console
 */
void console_putc(uint32_t c)
{
#if defined(CONSOLE_FB) | defined(CONSOLE_SERIAL)
    int i;
#endif
#ifdef CONSOLE_SERIAL
    uint8_t str[4];
#endif
#ifdef CONSOLE_FB
    uintptr_t o, p;
    uint32_t j, k, l, m, n, bpp = ((vidmode.framebuffer_bpp + 7) >> 3);
    uint8_t *ptr, *chr, *frg, *fb = (uint8_t*)vidmode.framebuffer_addr;

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
                            if(*frg & m)
                                switch(vidmode.framebuffer_bpp) {
                                    case 15: case 16: *((uint16_t*)p) = fb_fg; break;
                                    case 24: case 32: *((uint32_t*)p) = fb_fg; break;
                                }
                            else if(!(cons & 8))
                                switch(vidmode.framebuffer_bpp) {
                                    case 15: case 16: *((uint16_t*)p) = fb_bg; break;
                                    case 24: case 32: *((uint32_t*)p) = fb_bg; break;
                                }
                        }
                }
                fb_x += chr[4]; fb_y += chr[5];
            break;
        }
#endif
#ifdef CONSOLE_SERIAL
    if(cons & 1) {
        if(c<0x80) { str[0] = c; str[1] = 0; } else
        if(c<0x800) { str[0]=((c>>6)&0x1F)|0xC0; str[1]=(c&0x3F)|0x80; str[2] = 0; }
        else { str[0]=((c>>12)&0x0F)|0xE0; str[1]=((c>>6)&0x3F)|0x80; str[2]=(c&0x3F)|0x80; str[3] = 0; }
        if(str[0] == '\n') { str[0] = '\r'; str[1] = '\n'; }
        for(i = 0; str[i]; i++) {
            do{ __asm__ __volatile__("nop");} while(*UART0_FR&0x20); *UART0_DR=str[i];
        }
    }
#endif
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
        switch(vidmode.framebuffer_bpp) {
            case 15: case 16: *((uint16_t*)(pb_fb + i)) = *((uint16_t*)(pb_fb + vidmode.framebuffer_pitch + i)) = pb_bg; break;
            case 24: case 32: *((uint32_t*)(pb_fb + i)) = *((uint32_t*)(pb_fb + vidmode.framebuffer_pitch + i)) = pb_bg; break;
        }
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
            switch(vidmode.framebuffer_bpp) {
                case 15: case 16: *((uint16_t*)(pb_fb + i)) = *((uint16_t*)(pb_fb + vidmode.framebuffer_pitch + i)) = fb_fg; break;
                case 24: case 32: *((uint32_t*)(pb_fb + i)) = *((uint32_t*)(pb_fb + vidmode.framebuffer_pitch + i)) = fb_fg; break;
            }
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
            switch(vidmode.framebuffer_bpp) {
                case 15: case 16: *((uint16_t*)(pb_fb + i)) = *((uint16_t*)(pb_fb + vidmode.framebuffer_pitch + i)) = fb_bg; break;
                case 24: case 32: *((uint32_t*)(pb_fb + i)) = *((uint32_t*)(pb_fb + vidmode.framebuffer_pitch + i)) = fb_bg; break;
            }
    pb_fb = NULL;
}

/**************** EMMC SDCard functions ****************/

#define EMMC_ARG2           ((volatile uint32_t*)(emmc_base+0x00000000))
#define EMMC_BLKSIZECNT     ((volatile uint32_t*)(emmc_base+0x00000004))
#define EMMC_ARG1           ((volatile uint32_t*)(emmc_base+0x00000008))
#define EMMC_CMDTM          ((volatile uint32_t*)(emmc_base+0x0000000C))
#define EMMC_RESP0          ((volatile uint32_t*)(emmc_base+0x00000010))
#define EMMC_RESP1          ((volatile uint32_t*)(emmc_base+0x00000014))
#define EMMC_RESP2          ((volatile uint32_t*)(emmc_base+0x00000018))
#define EMMC_RESP3          ((volatile uint32_t*)(emmc_base+0x0000001C))
#define EMMC_DATA           ((volatile uint32_t*)(emmc_base+0x00000020))
#define EMMC_STATUS         ((volatile uint32_t*)(emmc_base+0x00000024))
#define EMMC_CONTROL0       ((volatile uint32_t*)(emmc_base+0x00000028))
#define EMMC_CONTROL1       ((volatile uint32_t*)(emmc_base+0x0000002C))
#define EMMC_INTERRUPT      ((volatile uint32_t*)(emmc_base+0x00000030))
#define EMMC_INT_MASK       ((volatile uint32_t*)(emmc_base+0x00000034))
#define EMMC_INT_EN         ((volatile uint32_t*)(emmc_base+0x00000038))
#define EMMC_CONTROL2       ((volatile uint32_t*)(emmc_base+0x0000003C))
#define EMMC_SLOTISR_VER    ((volatile uint32_t*)(emmc_base+0x000000FC))
#define CMD_NEED_APP        0x80000000
#define CMD_RSPNS_48        0x00020000
#define CMD_ERRORS_MASK     0xfff9c004
#define CMD_RCA_MASK        0xffff0000
#define CMD_GO_IDLE         0x00000000
#define CMD_ALL_SEND_CID    0x02010000
#define CMD_SEND_REL_ADDR   0x03020000
#define CMD_CARD_SELECT     0x07030000
#define CMD_SEND_IF_COND    0x08020000
#define CMD_STOP_TRANS      0x0C030000
#define CMD_READ_SINGLE     0x11220010
#define CMD_READ_MULTI      0x12220032
#define CMD_SET_BLOCKCNT    0x17020000
#define CMD_APP_CMD         0x37000000
#define CMD_SET_BUS_WIDTH   (0x06020000|CMD_NEED_APP)
#define CMD_SEND_OP_COND    (0x29020000|CMD_NEED_APP)
#define CMD_SEND_SCR        (0x33220010|CMD_NEED_APP)
#define SR_READ_AVAILABLE   0x00000800
#define SR_DAT_INHIBIT      0x00000002
#define SR_CMD_INHIBIT      0x00000001
#define SR_APP_CMD          0x00000020
#define INT_DATA_TIMEOUT    0x00100000
#define INT_CMD_TIMEOUT     0x00010000
#define INT_READ_RDY        0x00000020
#define INT_CMD_DONE        0x00000001
#define INT_ERROR_MASK      0x017E8000
#define C0_SPI_MODE_EN      0x00100000
#define C0_HCTL_HS_EN       0x00000004
#define C0_HCTL_DWITDH      0x00000002
#define C1_SRST             0x07000000
#define C1_SRST_DATA        0x04000000
#define C1_SRST_CMD         0x02000000
#define C1_SRST_HC          0x01000000
#define C1_TOUNIT_DIS       0x000f0000
#define C1_TOUNIT_MAX       0x000e0000
#define C1_CLK_GENSEL       0x00000020
#define C1_CLK_EN           0x00000004
#define C1_CLK_STABLE       0x00000002
#define C1_CLK_INTLEN       0x00000001
#define HOST_SPEC_NUM       0x00ff0000
#define HOST_SPEC_NUM_SHIFT 16
#define HOST_SPEC_V3        2
#define HOST_SPEC_V2        1
#define HOST_SPEC_V1        0
#define SCR_SD_BUS_WIDTH_4  0x00000400
#define SCR_SUPP_SET_BLKCNT 0x02000000
#define SCR_SUPP_CCS        0x00000001
#define ACMD41_VOLTAGE      0x00ff8000
#define ACMD41_CMD_COMPLETE 0x80000000
#define ACMD41_CMD_CCS      0x40000000
#define ACMD41_ARG_HC       0x51ff8000
#define SD_OK                0
#define SD_TIMEOUT          -1
#define SD_ERROR            -2

uint32_t sd_scr[2], sd_ocr, sd_rca, sd_hv;
int sd_err;

uint64_t cntfrq;
/* delay cnt microsec */
void delayms(uint32_t cnt) {
    uint64_t t,r;
    if(!cntfrq) __asm__ __volatile__ ("mrs %0, cntfrq_el0" : "=r" (cntfrq));
    __asm__ __volatile__ ("mrs %0, cntpct_el0" : "=r" (t));
    t+=((cntfrq/1000)*cnt)/1000;do{__asm__ __volatile__ ("mrs %0, cntpct_el0" : "=r" (r));}while(r<t);
}

/**
 * Wait for data or command ready
 */
int sd_status(uint32_t mask)
{
    int cnt = 500000; while((*EMMC_STATUS & mask) && !(*EMMC_INTERRUPT & INT_ERROR_MASK) && cnt--) delayms(1);
    return (cnt <= 0 || (*EMMC_INTERRUPT & INT_ERROR_MASK)) ? SD_ERROR : SD_OK;
}

/**
 * Wait for interrupt
 */
int sd_int(uint32_t mask)
{
    uint32_t r, m=mask | INT_ERROR_MASK;
    int cnt = 1000000; while(!(*EMMC_INTERRUPT & m) && cnt--) delayms(1);
    r=*EMMC_INTERRUPT;
    if(cnt<=0 || (r & INT_CMD_TIMEOUT) || (r & INT_DATA_TIMEOUT) ) { *EMMC_INTERRUPT=r; return SD_TIMEOUT; } else
    if(r & INT_ERROR_MASK) { *EMMC_INTERRUPT=r; return SD_ERROR; }
    *EMMC_INTERRUPT=mask;
    return 0;
}

/**
 * Send a command
 */
int sd_cmd(uint32_t code, uint32_t arg)
{
    uint32_t r=0;
    sd_err=SD_OK;
    if(code&CMD_NEED_APP) {
        r=sd_cmd(CMD_APP_CMD|(sd_rca?CMD_RSPNS_48:0),sd_rca);
        if(sd_rca && !r) { printf("EMMC: failed to send SD APP command\n"); sd_err=SD_ERROR;return 0;}
        code &= ~CMD_NEED_APP;
    }
    if(sd_status(SR_CMD_INHIBIT)) { printf("EMMC: busy, timed out\n"); sd_err= SD_TIMEOUT;return 0;}
    *EMMC_INTERRUPT=*EMMC_INTERRUPT; *EMMC_ARG1=arg; *EMMC_CMDTM=code;
    if(code==CMD_SEND_OP_COND) delayms(1000); else
    if(code==CMD_SEND_IF_COND || code==CMD_APP_CMD) delayms(100);
    if((r=sd_int(INT_CMD_DONE))) {printf("EMMC: failed to send command\n");sd_err=r;return 0;}
    r=*EMMC_RESP0;
    if(code==CMD_GO_IDLE || code==CMD_APP_CMD) return 0; else
    if(code==(CMD_APP_CMD|CMD_RSPNS_48)) return r&SR_APP_CMD; else
    if(code==CMD_SEND_OP_COND) return r; else
    if(code==CMD_SEND_IF_COND) return r==arg? SD_OK : SD_ERROR; else
    if(code==CMD_ALL_SEND_CID) {r|=*EMMC_RESP3; r|=*EMMC_RESP2; r|=*EMMC_RESP1; return r; } else
    if(code==CMD_SEND_REL_ADDR) {
        sd_err=(((r&0x1fff))|((r&0x2000)<<6)|((r&0x4000)<<8)|((r&0x8000)<<8))&CMD_ERRORS_MASK;
        return r&CMD_RCA_MASK;
    }
    return r&CMD_ERRORS_MASK;
}

/**
 * Load a sector from boot drive using EMMC
 */
int sd_loadsec(uint64_t lba, void *dst)
{
    int r,d;
    uint32_t *buf=(uint32_t *)dst;

#ifdef SD_DEBUG
    printf("EMMC: sd_loadsec lba %d dst %lx\n", lba, dst);
#endif
    if(sd_status(SR_DAT_INHIBIT)) {sd_err=SD_TIMEOUT; return 0;}
    *EMMC_BLKSIZECNT = (1 << 16) | 512;
    sd_cmd(CMD_READ_SINGLE, sd_scr[0] & SCR_SUPP_CCS ? lba : lba << 9);
    if(sd_err) return 0;
    if((r=sd_int(INT_READ_RDY))){printf("\rEMMC: timeout waiting for ready to read\n");sd_err=r;return 0;}
    for(d=0;d<128;d++) buf[d] = *EMMC_DATA;
    return sd_err!=SD_OK;
}

/**
 * set SD clock to frequency in Hz
 */
int sd_clk(uint32_t f)
{
    uint32_t d,c=41666666/f,x,s=32,h=0;
    int cnt = 100000;
    while((*EMMC_STATUS & (SR_CMD_INHIBIT|SR_DAT_INHIBIT)) && cnt--) delayms(1);
    if(cnt<=0) {printf("EMMC: timeout waiting for inhibit flag\n"); return SD_ERROR; }

    *EMMC_CONTROL1 &= ~C1_CLK_EN; delayms(10);
    x=c-1; if(!x) s=0; else {
        if(!(x & 0xffff0000u)) { x <<= 16; s -= 16; }
        if(!(x & 0xff000000u)) { x <<= 8;  s -= 8; }
        if(!(x & 0xf0000000u)) { x <<= 4;  s -= 4; }
        if(!(x & 0xc0000000u)) { x <<= 2;  s -= 2; }
        if(!(x & 0x80000000u)) { x <<= 1;  s -= 1; }
        if(s>0) s--;
        if(s>7) s=7;
    }
    if(sd_hv>HOST_SPEC_V2) d=c; else d=(1<<s);
    if(d<=2) {d=2;s=0;}
#ifdef SD_DEBUG
    printf("sd_clk divisor %x, shift %x\n", d, s);
#endif
    if(sd_hv>HOST_SPEC_V2) h=(d&0x300)>>2;
    d=(((d&0x0ff)<<8)|h);
    *EMMC_CONTROL1=(*EMMC_CONTROL1&0xffff003f)|d; delayms(10);
    *EMMC_CONTROL1 |= C1_CLK_EN; delayms(10);
    cnt=10000; while(!(*EMMC_CONTROL1 & C1_CLK_STABLE) && cnt--) delayms(10);
    if(cnt<=0) {printf("EMMC: failed to get stable clock\n");return SD_ERROR;}
    return SD_OK;
}

/**
 * initialize EMMC to read SDHC card
 */
int sd_init()
{
    long r,cnt,ccs=0;
    sd_hv = (*EMMC_SLOTISR_VER & HOST_SPEC_NUM) >> HOST_SPEC_NUM_SHIFT;
#ifdef SD_DEBUG
    printf("EMMC: GPIO set up hcl ver %d\n", sd_hv);
#endif
    if(sd_hv<HOST_SPEC_V2) {printf("EMMC: SDHCI version too old\n"); return SD_ERROR;}
    /* Reset the card. */
    *EMMC_CONTROL0 = 0;
    r = *EMMC_CONTROL1; r |= C1_SRST_HC; r &= ~(C1_CLK_EN | C1_CLK_INTLEN); *EMMC_CONTROL1 = r;
    cnt=10000; do{delayms(10);} while( (*EMMC_CONTROL1 & C1_SRST) && cnt-- );
    if(cnt<=0) {printf("EMMC: failed to reset EMMC\n"); return SD_ERROR;}
#ifdef SD_DEBUG
    printf("EMMC: reset OK\n");
#endif
    *EMMC_CONTROL0 = 0xF << 8; /* set voltage to 3.3 */
    *EMMC_CONTROL1 |= C1_CLK_INTLEN | C1_TOUNIT_MAX;
    delayms(10);
    /* Set clock to setup frequency. */
    if((r=sd_clk(400000))) return r;
    *EMMC_INT_EN   = 0xffffffff;
    *EMMC_INT_MASK = 0xffffffff;
    sd_scr[0]=sd_scr[1]=sd_rca=sd_err=0;
    sd_cmd(CMD_GO_IDLE,0);
    if(sd_err) return sd_err;

    sd_cmd(CMD_SEND_IF_COND,0x000001AA);
    if(sd_err) return sd_err;
    cnt=6; r=0; while(!(r&ACMD41_CMD_COMPLETE) && cnt--) {
        delayms(1);
        r=sd_cmd(CMD_SEND_OP_COND,ACMD41_ARG_HC);
#ifdef SD_DEBUG
        printf("EMMC: CMD_SEND_OP_COND returned ");
        if(r&ACMD41_CMD_COMPLETE) printf("COMPLETE ");
        if(r&ACMD41_VOLTAGE) printf("VOLTAGE ");
        if(r&ACMD41_CMD_CCS) printf("CCS ");
        printf("%x\n", r);
#endif
        if(sd_err!=SD_TIMEOUT && sd_err!=SD_OK ) {printf("EMMC: EMMC ACMD41 returned error\n"); return sd_err;}
    }
    if(!(r&ACMD41_CMD_COMPLETE) || !cnt ) return SD_TIMEOUT;
    if(!(r&ACMD41_VOLTAGE)) return SD_ERROR;
    if(r&ACMD41_CMD_CCS) ccs=SCR_SUPP_CCS;

    sd_cmd(CMD_ALL_SEND_CID,0);

    sd_rca = sd_cmd(CMD_SEND_REL_ADDR,0);
#ifdef SD_DEBUG
    printf("EMMC: CMD_SEND_REL_ADDR returned %x\n", sd_rca);
#endif
    if(sd_err) return sd_err;

    if((r=sd_clk(25000000))) return r;

    sd_cmd(CMD_CARD_SELECT,sd_rca);
    if(sd_err) return sd_err;

    if(sd_status(SR_DAT_INHIBIT)) return SD_TIMEOUT;
    *EMMC_BLKSIZECNT = (1<<16) | 8;
    sd_cmd(CMD_SEND_SCR,0);
    if(sd_err) return sd_err;
    if(sd_int(INT_READ_RDY)) return SD_TIMEOUT;

    r=0; cnt=100000; while(r<2 && cnt) {
        if( *EMMC_STATUS & SR_READ_AVAILABLE )
            sd_scr[r++] = *EMMC_DATA;
        else
            delayms(1);
    }
    if(r!=2) return SD_TIMEOUT;
    if(sd_scr[0] & SCR_SD_BUS_WIDTH_4) {
        sd_cmd(CMD_SET_BUS_WIDTH,sd_rca|2);
        if(sd_err) return sd_err;
        *EMMC_CONTROL0 |= C0_HCTL_DWITDH;
    }
    /* add software flag */
#ifdef SD_DEBUG
    printf("EMMC: supports ");
    if(sd_scr[0] & SCR_SUPP_SET_BLKCNT) printf("SET_BLKCNT ");
    if(ccs) printf("CCS ");
    printf("\n");
#endif
    sd_scr[0]&=~SCR_SUPP_CCS;
    sd_scr[0]|=ccs;
    return SD_OK;
}

/**************** Common functions ****************/

/**
 * Allocate and zero out pages
 */
PLG_API void *alloc(uint32_t num)
{
    uint64_t page = file_buf;
    file_buf += num * 4096;
    memset((void*)page, 0, num * 4096);
    return (void*)page;
}

/**
 * Free allocated pages
 */
PLG_API void free(void *buf, uint32_t num)
{
    (void)buf; (void)num;
}

/**
 * Load a sector from root (or boot) partition
 */
PLG_API void loadsec(uint64_t sec, void *dst)
{
    memset(dst, 0, 512);
    sd_loadsec(sec + (root_lba ? root_lba : (uint64_t)root_dir), dst);
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
 * Load a sector from boot drive using EMMC
 */
int fw_loadsec(uint64_t lba, void *dst)
{
    return sd_loadsec(lba, dst);
}

/**
 * Map virtual memory
 */
int fw_map(uint64_t phys, uint64_t virt, uint32_t size)
{
    uint64_t end = virt + size, *ptr, *next = NULL, orig = file_buf, type;

    /* is this a canonical address? We handle virtual memory up to 256TB */
    if(!pt || ((virt >> 48L) != 0x0000 && (virt >> 48L) != 0xffff)) return 0;
    /* if we're mapping the framebuffer, use device SH=2 memory type */
    type = 0x03 | (1<<10) | (file_buf < 0x20000 ? (2<<8) | (1<<2) | (1L<<54) : (3<<8));

    /* walk the page tables and add the missing pieces */
    for(virt &= ~4095, phys &= ~4095; virt < end; virt += 4096) {
        /* 1G */
        ptr = &pt[((virt >> 48L) ? 512 : 0) + ((virt >> 30L) & 511)];
        if(!*ptr) { if(!(*ptr = (uint64_t)alloc(1))) return 0; else *ptr |= 3|(3<<8)|(1<<10); }
        /* 2M if we previously had a large page here, split it into 4K pages */
        ptr = (uint64_t*)(*ptr & ~4095); ptr = &ptr[(virt >> 21L) & 511];
        if(!(*ptr & 0x2)) { if(!(*ptr = (uint64_t)alloc(1))) return 0; else *ptr |= 3|(3<<8)|(1<<10); }
        /* 4K */
        ptr = (uint64_t*)(*ptr & ~4095); ptr = &ptr[(virt >> 12L) & 511];
        /* if this page is already mapped, that means the kernel has invalid, overlapping segments */
        if(!*ptr || file_buf < 0x20000) { *ptr = (uint64_t)next; next = ptr; }
    }
    /* resolve the linked list */
    for(end = ((phys == orig ? file_buf : phys) + size - 1) & ~4095; next; end -= 4096, next = ptr) {
        ptr = (uint64_t*)*next; *next = end | type;
    }
    return 1;
}

/**
 * Initialize firmware related stuff
 */
void fw_init(void)
{
    uint64_t i=0, ms, me;

    /* get the base address */
    __asm__ __volatile__("mrs %0, midr_el1;":"=r"(mmio_base)::);
    switch(mmio_base & 0xFFF0) {
        case 0xD030: rpi = 3; mmio_base = 0x3F000000; emmc_base = 0x3F300000; break;     /* Raspberry Pi 3 */
        default:     rpi = 4; mmio_base = 0xFE000000; emmc_base = 0xFE340000; break;     /* Raspberry Pi 4 */
    }
    file_base = 16*1024*1024;
    /* check if we got a DTB from the firmware */
    dtb_base = (uint8_t*)0x40000;
    if((dtb_base)[0] != 0xD0 || (dtb_base)[1] != 0x0D || (dtb_base)[2] != 0xFE || (dtb_base)[3] != 0xED)
        dtb_base = 0;
    tags_buf = (uint8_t*)0x20000;
    plgids = NULL;
    /* initialize screen and console */
    mbox_lfb(0, 0, 0);
    fb_fg = FB_COLOR(127, 127, 127); fb_bg = 0; pb_bg = FB_COLOR(32, 32, 32);
    console_init();
    /* set up paging */
    ms = mmio_base >> 21; me = (mmio_base + 0x800000) >> 21;
    pt = (uint64_t*)0x1000;
    memset(pt, 0, 8 * 4096);
    /* TTBR0 */
    for(i = 0; i < 4; i++)
        pt[i] = (uintptr_t)pt + ((i + 2) * 4096) + (3|(3<<8)|(1<<10));
    for(i = 0; i < 4 * 512; i++) pt[1024 + i] = (uintptr_t)i * 2 * 1024 * 1024 +
        /* if we're mapping the mmio area, use device SH=2 memory type */
        (1 | (1<<10) | (i >= ms && i < me ? (2<<8) | (1<<2) | (1L<<54) : (3<<8)));
    /* dynamically map framebuffer */
    if(vidmode.framebuffer_addr) {
        /* map new pages from the page table area */
        file_buf = 0x7000;
        fw_map(vidmode.framebuffer_addr, vidmode.framebuffer_addr,
            (vidmode.framebuffer_pitch * vidmode.framebuffer_height + 4095) & ~4095);
    }
    file_buf = file_base;
    /* TTBR1 */
    /* pt[512] = nothing for now; */
}

/**
 * Initialize root file system
 */
void fw_fsinit(void)
{
    guid_t espGuid = EFI_PART_TYPE_EFI_SYSTEM_PART_GUID;
    uint64_t i, j, k, l, n;

    /* initialize SDCard */
    sd_init();
    /* get boot partition's root directory */
    fw_loadsec(1, &vbr);
    root_dir = 0;
    if(!memcmp(&vbr, EFI_PTAB_HEADER_ID, 8)) {
        /* found GPT */
        j = ((gpt_header_t*)&vbr)->SizeOfPartitionEntry;
        l = ((gpt_header_t*)&vbr)->PartitionEntryLBA;
        n = ((gpt_header_t*)&vbr)->NumberOfPartitionEntries;
        /* look for ESP */
        for(k = 0; n && !root_dir; k++) {
            fw_loadsec(l + k, &vbr);
            for(i = 0; n && i + j <= 512; i += j, n--) {
                /* does ESP type match? */
                if(!memcmp(&((gpt_entry_t*)&vbr[i])->PartitionTypeGUID, &espGuid, sizeof(guid_t))) {
                    root_dir = ((gpt_entry_t*)&vbr[i])->StartingLBA;
                    break;
                }
            }
        }
    } else {
        /* fallback to MBR partitioning scheme */
        fw_loadsec(0, &vbr);
        if(vbr[510] == 0x55 && vbr[511] == 0xAA)
            for(i = 0x1c0; i < 510; i += 16)
                if(vbr[i - 2] == 0x80/*active*/ && (vbr[i + 2] == 0xC/*FAT32*/ || vbr[i + 2] == 0xEF/*ESP*/)) {
                    root_dir = (uint64_t)(*((uint32_t*)&vbr[i + 6]));
                    break;
                }
    }
    if(root_dir) {
        fw_loadsec(root_dir, &vbr);
        bpb = (esp_bpb_t*)&vbr;
        if(vbr[510] != 0x55 || vbr[511] != 0xAA || bpb->bps != 512 || !bpb->spc || bpb->spf16 || !bpb->spf32)
            root_dir = 0;
        else {
            /* calculate the LBA address of the FAT and the first data sector */
            fat_lba = bpb->rsc + (uint64_t)root_dir;
            data_lba = bpb->spf32 * bpb->nf + bpb->rsc - 2 * bpb->spc + (uint64_t)root_dir;
            /* load the beginning of the FAT into the cache */
            for(i = 0; i < 8; i++) fw_loadsec(fat_lba + i, &fat[i << 7]);
            fat_cache = 0;
        }
    }
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
 * Get the next cluster from FAT
 */
uint32_t fw_nextclu(uint32_t clu)
{
    uint64_t i;

    if(clu < 2 || clu >= 0x0FFFFFF8) return 0;
    if(clu < fat_cache || clu > fat_cache + 1023) {
        fat_cache = clu & ~1023;
        for(i = 0; i < 8; i++) fw_loadsec(fat_lba + (fat_cache >> 7) + i, &fat[i << 7]);
    }
    clu = fat[clu - fat_cache];
    return clu < 2 || clu >= 0x0FFFFFF8 ? 0 : clu;
}

/**
 * Open a file
 */
PLG_API int fw_open(char *fn)
{
    uint64_t lba;
    uint32_t clu = bpb->rc, c, l;
    int i, n = 0, m = 0, rd = 0;
    uint8_t secleft = 0, *dir = data + sizeof(data), *plgs = plgids, *p;
    uint16_t *u, *s = wcname, a, b, *d;
    char *S;

    file_size = file_clu = 0;
    if(!fn || !*fn) return 0;
    /* UTF-8 to WCHAR */
    for(S = fn, d = wcname; *S && *S != ' ' && *S != '}' && *S != '\r' && *S != '\n' && d < &wcname[PATH_MAX - 2]; d++) {
        c = utf8(&S); if(c == '\\' && *S == ' ') { c = ' '; S++; }
        *d = c;
    }
    *d = 0;

    memset(lfn, 0, sizeof(lfn));
    while(1) {
        /* have we reached the end of the sector? */
        if(dir >= data + sizeof(data)) {
            if(secleft) { secleft--; lba++; }
            else {
                if(clu < 2 || clu >= 0x0FFFFFF8) return 0;
                secleft = bpb->spc - 1;
                lba = clu * bpb->spc + data_lba;
                clu = fw_nextclu(clu);
            }
            fw_loadsec(lba, &data);
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
                        fw_loadsec(c * bpb->spc + data_lba, plgs);
                        if(!memcmp(plgs, PLG_MAGIC, 4) && (((plg_hdr_t*)plgs)->nid || ((plg_hdr_t*)plgs)->type == PLG_T_TAG) &&
                          ((plg_hdr_t*)plgs)->arch == EM_AARCH64) {
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
 * Read data from file
 */
PLG_API uint64_t fw_read(uint64_t offs, uint64_t size, void *buf)
{
    uint64_t lba = 0, rem, o;
    uint32_t clu = file_clu, nc, ns = 0, os = 0, rs = 512;
    uint8_t secleft = 0;

    if(file_clu < 2 || offs >= file_size || !size || !buf) return 0;
    if(offs + size > file_size) size = file_size - offs;
    rem = size;

    pb_init(size);
    if(offs) {
        nc = offs / (bpb->spc << 9); o = offs % (bpb->spc << 9);
        ns = o >> 9; os = o & 0x1ff; rs = 512 - os;
        if(nc) { while(nc-- && clu) { clu = fw_nextclu(clu); } if(!clu) return 0; }
        secleft = bpb->spc - ns - 1;
        lba = clu * bpb->spc + ns - 1 + data_lba;
    }
    while(rem) {
        if(secleft) { secleft--; lba++; }
        else {
            if(!clu) break;
            secleft = bpb->spc - 1;
            lba = clu * bpb->spc + data_lba;
            clu = fw_nextclu(clu);
        }
        if(rs > rem) rs = rem;
        if(rs < 512) {
            fw_loadsec(lba, data);
            memcpy(buf, data + os, rs); os = 0;
        } else {
            fw_loadsec(lba, buf);
            if(os) { memcpy(buf, buf + os, rs); os = 0; }
        }
        buf += rs; rem -= rs; rs = 512;
        pb_draw(size - rem);
    }
    pb_fini();
    return (size - rem);
}

/**
 * Close file
 */
PLG_API void fw_close(void)
{
    file_clu = 0;
}

/**
 * Load list of available plugins
 */
void fw_loadplugins(void)
{
    plgids = (uint8_t*)0x80000;
    rddir = 1; memcpy(fn, "easyboot/", 10);
    if(!fw_open(fn)) { rddir = 0; return; }
    fw_close();
    rddir = 0;
}

/**
 * Find the root partition
 */
int fw_findroot(void)
{
    uint64_t i, j, k, l, n;

    fw_loadsec(1, data);
    if(!memcmp(&data, EFI_PTAB_HEADER_ID, 8)) {
        j = ((gpt_header_t*)&data)->SizeOfPartitionEntry;
        l = ((gpt_header_t*)&data)->PartitionEntryLBA;
        n = ((gpt_header_t*)&data)->NumberOfPartitionEntries;
        for(k = 0; n && !root_dir; k++) {
            fw_loadsec(l + k, data);
            for(i = 0; n && i + j <= 512; i += j, n--) {
                if(!memcmp(&((gpt_entry_t*)&data[i])->UniquePartitionGUID, &rootuuid, sizeof(guid_t))) {
                    root_buf = (uint8_t*)alloc(32); if(!root_buf) return 0;
                    root_lba = ((gpt_entry_t*)&data[i])->StartingLBA;
                    root_siz = ((gpt_entry_t*)&data[i])->EndingLBA - root_lba + 1;
                    for(i = 0; i < 256; i++)
                        fw_loadsec(root_lba + i, (void*)(root_buf + (i << 9)));
                    return 1;
                }
            }
        }
    }
    return 0;
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

    if(!buf || !plgids) return NULL;
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
    uint32_t t;
#ifdef CONSOLE_SERIAL
    uint32_t k;
#endif
    /* progressbar is only shown if the total value is bigger than the screen width, so be sure, multiply by 10000 */
    pb_init(timeout * 10000); t = 0;
    do {
        /* wait a bit */
        delayms(1000);
        /* should check keypress on USB keyboard too, but that alone is a bigger project than this entire loader... */
#ifdef CONSOLE_SERIAL
        /* check key on serial */
        if(!(*UART0_FR & 0x10)) {
            k = (uint8_t)(*UART0_DR);
            if(k >= '1' && k <= '9') return k - '0';
            /* we might have received an escape sequence */
            if(k == 27) {
                /* CSI sequences also have numbers in them, but we must skip those */
                do {
                    do{ __asm__ __volatile__ ("nop"); }while(*UART0_FR&0x10); k = (uint8_t)(*UART0_DR);
                } while(k == '[' || k == ':' || k == ';' || k == ',' || (k >= '0' && k <= '9'));
            }
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
    uint8_t *d = NULL;
    uint32_t siz;
    char *c = fn;

    if(!path || !*path) { file_size = 0; return NULL; }
    if(*path == '/') path++;
    for(; c < fn + sizeof(lfn) - 1 && *path && *path != ' ' && *path != '}' && *path != '\r' && *path != '\n'; path++) {
        if(*path == '\\' && path[1] == ' ') path++;
        *c++ = *path;
    }
    *c = 0;

    if((*plg_got.open)(fn)) {
        if(verbose) printf("Loading '%s' (%ld bytes)...\n", fn, file_size);
        d = (uint8_t*)file_buf;
        file_buf += (file_size + 4095) & ~4095;
        if(d) {
            (*plg_got.read)(0, file_size, d);
            /* if it's gzip compressed */
            if((d[0] == 0x1f && d[1] == 0x8b) || (d[0] == 'G' && d[1] == 'U' && d[1] == 'D' && d[8] == 0x78)) {
                if(d[0] == 0x1f) memcpy(&siz, d + file_size - 4, 4);
                else siz = (((d[4] | (d[5] << 8)) + 7) & ~7) + ((d[6] | (d[7] << 8)) << 4);
                if(verbose) printf("Uncompressing (%ld bytes)...\n", siz);
                uncompress(d, (uint8_t*)file_buf, siz);
                memcpy(d, (uint8_t*)file_buf, siz);
                file_size = siz;
                file_buf = (uint64_t)d + (siz + 4095) & ~4095;
            } else
            /* if we have a plugin to decompress it */
            if((plg = fw_findplg(PLG_T_DECOMP, d)))
                d = (*((uint8_t*(* PLG_API)(uint8_t*))((uintptr_t)plg->entry)))(d);
        }
        (*plg_got.close)();
    }
    return d;
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
    uint8_t ma = 0;
    uint32_t i;
#endif

    defent = nument = l = 0; defto = 60000; conf_ptr = kernel = cmdline = NULL; memset(&rootuuid, 0, sizeof(guid_t));
    if(fw_open("easyboot/menu.cfg")) {
        conf_buf = (char*)tags_buf;
        tags_buf += (file_size + 7) & ~7;
        fw_read(0, file_size, conf_buf);
        conf_buf[file_size] = 0;
        fw_close();

#ifdef CONSOLE_FB
        /* when we have a config try to load a UNICODE font too, because we only have an ASCII one embedded */
        if((d = fw_loadfile("easyboot/font.sfn")) && !memcmp(d, "SFN2", 4) && (((ssfn_t*)d)->type & 15) == 3)
            font = (ssfn_t*)d;
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
                    mbox_lfb(w, h, p);
                    if(!vidmode.framebuffer_addr) mbox_lfb(r, g, b);
                    /* dynamically map framebuffer */
                    if(vidmode.framebuffer_addr) {
                        /* map new pages from the page table area */
                        d = (uint8_t*)file_buf; file_buf = 0x7000;
                        fw_map(vidmode.framebuffer_addr, vidmode.framebuffer_addr,
                            (vidmode.framebuffer_pitch * vidmode.framebuffer_height + 4095) & ~4095);
                        file_buf = (uint64_t)d;
                    }
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
                for(g = 0; a < e;) {
                    p = utf8(&a);
                    for(d = (uint8_t*)font + font->characters_offs, i = 0; i < 0x110000; i++) {
                        if(d[0] == 0xFF) { i += 65535; d++; }
                        else if((d[0] & 0xC0) == 0xC0) { i += (((d[0] & 0x3F) << 8) | d[1]); d += 2; }
                        else if((d[0] & 0xC0) == 0x80) { i += (d[0] & 0x3F); d++; }
                        else { if(i == p) { g += d[4]; break; } d += 6 + d[1] * (d[0] & 0x40 ? 6 : 5); }
                    }
                }
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
                    for(g = font->width, d = (uint8_t*)font + font->characters_offs, i = 0; i < 0x110000; i++) {
                        if(d[0] == 0xFF) { i += 65535; d++; }
                        else if((d[0] & 0xC0) == 0xC0) { i += (((d[0] & 0x3F) << 8) | d[1]); d += 2; }
                        else if((d[0] & 0xC0) == 0x80) { i += (d[0] & 0x3F); d++; }
                        else { if(i == r + '0') { g = d[4]; break; } d += 6 + d[1] * (d[0] & 0x40 ? 6 : 5); }
                    }
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
                        for(g = 0, s = a; s < e;) {
                            p = utf8(&s);
                            for(d = (uint8_t*)font + font->characters_offs, i = 0; i < 0x110000; i++) {
                                if(d[0] == 0xFF) { i += 65535; d++; }
                                else if((d[0] & 0xC0) == 0xC0) { i += (((d[0] & 0x3F) << 8) | d[1]); d += 2; }
                                else if((d[0] & 0xC0) == 0x80) { i += (d[0] & 0x3F); d++; }
                                else { if(i == p) { g += d[4]; break; } d += 6 + d[1] * (d[0] & 0x40 ? 6 : 5); }
                            }
                        }
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
        font = (ssfn_t*)font_sfn;
#endif
        if(!h) fw_clrscr();
        if(verbose) { cons = 2; printf((char*)copy); cons = 3; }
    }
    if(!kernel) kernel = (char*)defkernel;
}

/**
 * Detect config file independent configuration and generate tags for them
 */
void fw_loadsetup()
{
    multiboot_tag_loader_t *stag;
    multiboot_tag_mmap_t *mtag;
    char *c;

    file_buf = file_base;
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
        mbox[0]=8*4;
        mbox[1]=0;
        mbox[2]=0x10005; /* get memory size */
        mbox[3]=8;
        mbox[4]=0;
        mbox[5]=0;
        mbox[6]=0;
        mbox[7]=0;
        if(mbox_call(MBOX_CH_PROP)) ram = mbox[6];
        else ram = 256*1024*1024;
        ram &= ~(2 * 1024 * 1024 - 1);
        /* generate memory map tag */
        mtag = (multiboot_tag_mmap_t*)tags_ptr;
        mtag->type = MULTIBOOT_TAG_TYPE_MMAP;
        mtag->entry_size = sizeof(multiboot_mmap_entry_t);
        mtag->entry_version = 0;
        mtag->entries[0].base_addr = 0;
        mtag->entries[0].length = 0x400;
        mtag->entries[0].type = MULTIBOOT_MEMORY_RESERVED;
        mtag->entries[0].reserved = 0;
        mtag->entries[1].base_addr = 0x400;
        mtag->entries[1].length = ram - 0x400;
        mtag->entries[1].type = MULTIBOOT_MEMORY_AVAILABLE;
        mtag->entries[1].reserved = 0;
        mtag->entries[2].base_addr = mmio_base;
        mtag->entries[2].length = 0x00800000;
        mtag->entries[2].type = MULTIBOOT_MEMORY_RESERVED;
        mtag->entries[2].reserved = EfiMemoryMappedIO;
        mtag->size = sizeof(multiboot_tag_mmap_t) + 3 * sizeof(multiboot_mmap_entry_t);
        tags_ptr += (mtag->size + 7) & ~7;
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
                        dtb_base = ptr;
                        /* make sure we have enough space after the dtb, because we'll have to patch it */
                        file_buf += 4096;
                    } else
                    if(((ptr[0] == 'D' && ptr[1] == 'S') || (ptr[0] == 'G' && ptr[1] == 'U')) && ptr[2] == 'D' && ptr[3] == 'T') {
                        if(verbose) printf("%c%cDT detected...\n", ptr[0], ptr[1]);
                        dtb_base = ptr;
                    } else {
                        if(tags_ptr) {
                            tag = (multiboot_tag_module_t*)tags_ptr;
                            tag->type = MULTIBOOT_TAG_TYPE_MODULE;
                            tag->size = sizeof(multiboot_tag_module_t) + e - a + 1;
                            tag->mod_start = (uint32_t)(uintptr_t)ptr;
                            tag->mod_end = (uint32_t)(uintptr_t)ptr + (uint32_t)file_size;
                            memcpy(tag->string, a, e - a); tag->string[e - a] = 0;
                            if(verbose > 2) printf("%D\n", tag->mod_start);
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
 * Load a kernel segment
 */
PLG_API int fw_loadseg(uint32_t offs, uint32_t filesz, uint64_t vaddr, uint32_t memsz)
{
    uint8_t *buf = (uint8_t*)(uintptr_t)vaddr;
    uint32_t size;

    if(verbose > 1) printf("  segment %08x[%08x] -> %08x[%08x]\n", offs, filesz, vaddr, memsz);
    if(!memsz || !krnl_size || offs + filesz > krnl_size) { printf("ERROR: out of bound segment\n"); return 1; }
    size = (memsz + (vaddr & 4095) + 4095) & ~4095;
    /* no overwriting of the loader data */
    if(vaddr < _bss_end) goto err;
    if(vaddr > ram) {
        /* possibly a higher-half kernel's segment, we must map it */
        file_buf = (file_buf + 4095) & ~4095;
        if(!fw_map(file_buf, vaddr, size)) goto err;
        buf = (void*)file_buf; file_buf += size;
        /* if we have updated the paging tables, kick the MMU */
        __asm__ __volatile__("dsb ish; isb; mrs x0, sctlr_el1; msr sctlr_el1, x0; isb" :::"x0");
    }
    if(filesz) memcpy(buf, krnl_buf + offs, filesz);
    if(memsz > filesz) memset(buf + filesz, 0, memsz - filesz);
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
        if(((Elf64_Ehdr*)p)->e_machine != EM_AARCH64) { bad: printf("ERROR: bad architecture\n"); goto err; }
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
        offs = (uint32_t)pe->data.pe64.img_base;
        ent = offs + (uint8_t*)(uintptr_t)pe->entry_point;
        if(verbose) printf("Starting Multiboot2 PE32+ kernel...\n");
        if(((pe_hdr*)(p + ((mz_hdr*)p)->peaddr))->machine != IMAGE_FILE_MACHINE_ARM64) goto bad;
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
    if(smp) { *((volatile uint64_t*)0x538) = (uint64_t)(uintptr_t)ent; __asm__ __volatile__("dsb ish":::"memory"); }
    /* execute 64-bit kernels in Aarch64 mode */
    __asm__ __volatile__(
    "ldr x0, =tags_buf; ldr x1, [x0], #0;"  /* MBI tags pointer */
    "ldr x0, =0x36d76289;"                  /* magic */
    "mov sp, #0x80000; mov x30, %0; ret"    /* stack and jump to entry point */
    ::"r"(ent):);
    return 1;
}

/**
 * Finish up MBI tags
 */
void fw_fini(void)
{
    rsdp_t *rsdp;
    register uint64_t reg;
    uint32_t totalsize, off_dt, off_str, siz_str, siz_dt, tag, sz, ol = 0, nl = 0, cl = 0;
    uint8_t s, *p, *end, *n = NULL, *cmd = NULL;
    int i;

    /* patch the command line in the dtb, Linux needs this */
    if(dtb_base && (dtb_base[0] == 0xD0 && dtb_base[1] == 0x0D && dtb_base[2] == 0xFE && dtb_base[3] == 0xED)) {
        /* proof that Linux developers are incompetent and total morons: where did they put the most likely to change string?
         * in the string table maybe? Nah, that would be outrageous! They barfed that right in the middle of a binary blob!!! */
        totalsize = dtb_base[7] | (dtb_base[6] << 8) | (dtb_base[5] << 16);
        off_dt = dtb_base[11] | (dtb_base[10] << 8) | (dtb_base[9] << 16);
        off_str = dtb_base[15] | (dtb_base[14] << 8) | (dtb_base[13] << 16);
        siz_str = dtb_base[35] | (dtb_base[34] << 8) | (dtb_base[33] << 16);
        siz_dt = dtb_base[39] | (dtb_base[38] << 8) | (dtb_base[37] << 16);
        p = dtb_base + off_dt; end = dtb_base + off_dt + siz_dt;
        /* failsafe, we assume that the string table is the last section in the blob */
        if(off_str >= off_dt + siz_dt) {
            /* get size of the new command line */
            if(cmdline && *cmdline) for(; cmdline[cl]; cl++);
            cl++; nl = (cl + 3) & ~3;
            /* locate the command line node property */
            while(p < end) {
                tag = p[3] | (p[2] << 8) | (p[1] << 16) | (p[0] << 24); p += 4;
                if(tag == 9) break;
                if(tag == 1) {
                    for(n = p; p < end && *p; p++);
                    p = (uint8_t*)(((uintptr_t)p + 3) & ~3);
                } else
                if(tag == 2) n = NULL; else
                if(tag == 3) {
                    sz = p[3] | (p[2] << 8) | (p[1] << 16); p += 4; tag = p[3] | (p[2] << 8) | (p[1] << 16); p += 4;
                    if(tag < siz_str - 1 && n && !memcmp(n, "chosen", 7) && !memcmp(dtb_base + off_str + tag, "bootargs", 9)) {
                        cmd = p; ol = (sz + 3) & ~3; break;
                    }
                    p = (uint8_t*)(((uintptr_t)p + sz + 3) & ~3);
                }
            }
            /* if we haven't found it */
            if(!cmd) {
                /* we need to add the property key to the string table, and a chosen node at the end of the node tree */
                tag = siz_str; siz_str += 9; totalsize += 9;
                memcpy(dtb_base + tag, "bootargs", 9);
                cmd = dtb_base + off_dt + siz_dt;
                for(p = dtb_base + totalsize, n = dtb_base + totalsize + nl + 28; p >= cmd; p++, n++) *n = *p;
                memcpy(cmd, "\0\0\0\1chosen\0\0\0\0\0\3", 16); cmd += 16;
                cmd[3] = cl & 0xff; cmd[2] = (cl >> 8) & 0xff; cmd[1] = (cl >> 8) & 0xff; cmd[0] = 0; cmd += 4;
                cmd[3] = tag & 0xff; cmd[2] = (tag >> 8) & 0xff; cmd[1] = (tag >> 8) & 0xff; cmd[0] = 0; cmd += 4;
                if(cmdline) memcpy(cmd, cmdline, cl - 1);
                memset(cmd + cl - 1, 0, nl - cl + 1);
                memcpy(cmd + nl, "\0\0\0\2", 4);
                siz_dt += nl + 28; totalsize += nl + 28; off_str += nl + 28;
            } else {
                /* update the command line somewhere in the middle... */
                if(nl < ol) memcpy(cmd + nl, cmd + ol, totalsize - ol); else
                if(nl > ol) for(p = dtb_base + totalsize, n = dtb_base + totalsize + nl - ol; p >= cmd + ol; p++, n++) *n = *p;
                cmd[-5] = cl & 0xff; cmd[-6] = (cl >> 8) & 0xff; cmd[-7] = (cl >> 8) & 0xff; cmd[-8] = 0;
                if(cmdline) memcpy(cmd, cmdline, cl - 1);
                memset(cmd + cl - 1, 0, nl - cl + 1);
                siz_dt += nl - ol; totalsize += nl - ol; off_str += nl - ol;
            }
            /* write header back with new offset and size values */
            dtb_base[7] = totalsize & 0xff; dtb_base[6] = (totalsize >> 8) & 0xff; dtb_base[5] = (totalsize >> 16) & 0xff;
            dtb_base[15] = off_str & 0xff; dtb_base[14] = (off_str >> 8) & 0xff; dtb_base[13] = (off_str >> 16) & 0xff;
            dtb_base[35] = siz_str & 0xff; dtb_base[34] = (siz_str >> 8) & 0xff; dtb_base[33] = (siz_str >> 16) & 0xff;
            dtb_base[39] = siz_dt & 0xff; dtb_base[38] = (siz_dt >> 8) & 0xff; dtb_base[37] = (siz_dt >> 16) & 0xff;
        }
    }

    /* clear SMP semaphor */
    *((uint64_t*)0x538) = 0;
    if(tags_ptr) {
        if(vidmode.framebuffer_addr) {
            vidmode.type = MULTIBOOT_TAG_TYPE_FRAMEBUFFER;
            vidmode.size = sizeof(vidmode);
            vidmode.framebuffer_type = 1;
            vidmode.reserved = 0;
            memcpy(tags_ptr, &vidmode, vidmode.size);
            tags_ptr += (vidmode.size + 7) & ~7;
        }
        /* Get EDID info */
        mbox[0]=8*4;
        mbox[1]=0;
        mbox[2]=0x30020; /* get edid */
        mbox[3]=4;
        mbox[4]=0;
        mbox[5]=0;
        mbox[6]=0;
        mbox[7]=0;
        if(mbox_call(MBOX_CH_PROP) && mbox[3] > 128) {
            ((multiboot_tag_edid_t*)tags_ptr)->type = MULTIBOOT_TAG_TYPE_EDID;
            ((multiboot_tag_edid_t*)tags_ptr)->size = mbox[3];
            memcpy(tags_ptr + 8, (void*)&mbox[6], mbox[3] - 8);
            tags_ptr += (mbox[3] + 7) & ~7;
        }
        /* create a fake ACPI table */
        ((multiboot_tag_old_acpi_t*)tags_ptr)->type = MULTIBOOT_TAG_TYPE_ACPI_OLD;
        ((multiboot_tag_old_acpi_t*)tags_ptr)->size = sizeof(multiboot_tag_old_acpi_t) + sizeof(rsdp_t);
        rsdp = (rsdp_t*)&((multiboot_tag_old_acpi_t*)tags_ptr)->rsdp;
        memset(rsdp, 0, sizeof(rsdp_t));
        memcpy(&rsdp->magic, "RSD PTR ", 8);
        rsdp->rev = 1; rsdp->rsdt = (uint32_t)(uintptr_t)&rsdt;
        for(s = 0, i = 0; i < (int)sizeof(rsdp_t); i++) { s += *(((uint8_t*)rsdp) + i); } rsdp->chksum = 0x100 - s;
        memset(&rsdt, 0, sizeof(rsdt_t));
        memcpy(&rsdt.hdr.magic, "RSDT", 4);
        rsdt.hdr.rev = 1; rsdt.hdr.size = sizeof(sdt_hdr_t) + sizeof(uint32_t); rsdt.table_ptr[0] = (uint32_t)(uintptr_t)&apic;
        if(dtb_base && (
          (dtb_base[0] == 0xD0 && dtb_base[1] == 0x0D && dtb_base[2] == 0xFE && dtb_base[3] == 0xED) ||
          (((dtb_base[0] == 'D' && dtb_base[1] == 'S') || (dtb_base[0] == 'G' && dtb_base[1] == 'U')) &&
          dtb_base[2] == 'D' && dtb_base[3] == 'T'))) {
            /* add fake FADT and DSDT tables to the ACPI list with the DTB data */
            rsdt.hdr.size += sizeof(uint32_t);
            rsdt.table_ptr[1] = (uint32_t)(uintptr_t)&fadt; /* DSDT is pointed by FADT, not RSDT */
            memset(&fadt, 0, sizeof(fadt_t));
            memcpy(&fadt.hdr.magic, "FACP", 4);
            fadt.hdr.rev = 1; fadt.hdr.size = sizeof(fadt_t); fadt.dsdt = (uint32_t)(uintptr_t)dtb_base;
            for(s = 0, i = 0; i < (int)sizeof(fadt); i++) { s += *(((uint8_t*)&fadt) + i); } fadt.hdr.chksum = 0x100 - s;
        }
        for(s = 0, i = 0; i < (int)sizeof(rsdt_t); i++) { s += *(((uint8_t*)&rsdt) + i); } rsdt.hdr.chksum = 0x100 - s;
        memset(&apic, 0, sizeof(apic_t));
        memcpy(&apic.hdr.magic, "APIC", 4);
        apic.hdr.rev = 1; apic.hdr.size = sizeof(apic_t);
        for(i = 0; i < 4; i++) {
            apic.cpus[i].type = 0; apic.cpus[i].size = sizeof(cpu_entry_t);
            apic.cpus[i].acpi_id = i; apic.cpus[i].apic_id =  0xd8 + i * 8; apic.cpus[i].flags = i ? 2 : 1;
        }
        for(s = 0, i = 0; i < (int)sizeof(apic); i++) { s += *(((uint8_t*)&apic) + i); } apic.hdr.chksum = 0x100 - s;
        tags_ptr += (((multiboot_tag_old_acpi_t*)tags_ptr)->size + 7) & ~7;
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
        tags_ptr += (((multiboot_tag_t*)tags_ptr)->size + 7) & ~7;
        ((multiboot_info_t*)tags_buf)->total_size = tags_ptr - tags_buf;
    } else tags_buf = NULL;
    *((uint64_t*)0x540) = (uintptr_t)tags_buf;

    /* enable paging */
    reg=(0xFF << 0) |    /* Attr=0: normal, IWBWA, OWBWA, NTR */
        (0x04 << 8) |    /* Attr=1: device, nGnRE (must be OSH too) */
        (0x44 <<16);     /* Attr=2: non cacheable */
    __asm__ __volatile__("msr mair_el1, %0" ::"r"(reg):);
    *((uint64_t*)0x518) = reg;
    __asm__ __volatile__("mrs %0, id_aa64mmfr0_el1" :"=r"(reg)::);
    reg = (reg & 0xF) << 32L; /* IPS=autodetected */
    reg=(0x00LL << 37) | /* TBI=0, no tagging */
        (0x02LL << 30) | /* TG1=4k */
        (0x03LL << 28) | /* SH1=3 inner */
        (0x01LL << 26) | /* ORGN1=1 write back */
        (0x01LL << 24) | /* IRGN1=1 write back */
        (0x00LL << 23) | /* EPD1 undocumented by ARM DEN0024A Fig 12-5, 12-6 */
        (25LL   << 16) | /* T1SZ=25, 3 levels (512G) */
        (0x00LL << 14) | /* TG0=4k */
        (0x03LL << 12) | /* SH0=3 inner */
        (0x01LL << 10) | /* ORGN0=1 write back */
        (0x01LL << 8) |  /* IRGN0=1 write back */
        (0x00LL << 7) |  /* EPD0 undocumented by ARM DEN0024A Fig 12-5, 12-6 */
        (25LL   << 0);   /* T0SZ=25, 3 levels (512G) */
    __asm__ __volatile__("msr tcr_el1, %0; isb" ::"r" (reg):);
    *((uint64_t*)0x520) = reg;
    __asm__ __volatile__("msr ttbr0_el1, %0" ::"r"((uintptr_t)pt + 1):);
    *((uint64_t*)0x528) = (uintptr_t)pt + 1;
    __asm__ __volatile__("msr ttbr1_el1, %0" ::"r"((uintptr_t)pt + 1 + 4096):);
    *((uint64_t*)0x530) = (uintptr_t)pt + 1 + 4096;
    /* set mandatory reserved bits */
    __asm__ __volatile__("dsb ish; isb; mrs %0, sctlr_el1" :"=r"(reg)::);
    reg |= 0xC00801;
    reg&=~( (1<<25) |   /* clear EE, little endian translation tables */
            (1<<24) |   /* clear E0E */
            (1<<19) |   /* clear WXN */
            (1<<12) |   /* clear I, no instruction cache */
            (1<<4) |    /* clear SA0 */
            (1<<3) |    /* clear SA */
            (1<<2) |    /* clear C, no cache at all */
            (1<<1));    /* clear A, no aligment check */
    __asm__ __volatile__("msr sctlr_el1, %0; isb" ::"r"(reg):);
    *((uint64_t*)0x508) = reg;
}

/**
 * Dummy exception handler
 */
void fw_exc(uint8_t excno, uint64_t esr, uint64_t elr, uint64_t spsr, uint64_t far, uint64_t sctlr, uint64_t tcr)
{
    register uint64_t r0, r1;
#ifdef CONSOLE_FB
    uint32_t i, j, x, y, b;
#endif

    if(!in_exc) {
        in_exc++;
        /* only report exceptions for the BSP */
        __asm__ __volatile__ ("mrs x8, mpidr_el1; and x8, x8, #3; cbz x8, 2f; 1: wfe; b 1b; 2:;" :::"x8");
        __asm__ __volatile__ ("msr ttbr0_el1, %0;tlbi vmalle1" ::"r"((uint64_t)&pt[0] + 1));
        __asm__ __volatile__ ("dsb ish; isb; mrs %0, sctlr_el1" :"=r"(r0)::);
        /* set mandatory reserved bits to disable cache */
        r0 &= ~((1 << 12) /* clear I */ | (1 << 2) /* clear C */);
        __asm__ __volatile__ ("msr sctlr_el1, %0; isb" ::"r"(r0));

#ifdef CONSOLE_FB
        if(vidmode.framebuffer_addr) {
            font = (ssfn_t*)font_sfn;
            b = (vidmode.framebuffer_bpp + 7) >> 3;
            fb_x = fb_y = 4; fb_fg = FB_COLOR(255, 255, 255); fb_bg = FB_COLOR(255, 0, 0);
            for(j = y = 0; y < (8 + ((uint32_t)font->height << 1)); y++, j += vidmode.framebuffer_pitch)
                for(i = j, x = 0; x < vidmode.framebuffer_width; x++, i += b)
                    if(b == 2) *((uint16_t*)(vidmode.framebuffer_addr + i)) = (uint16_t)fb_bg;
                    else *((uint32_t*)(vidmode.framebuffer_addr + i)) = fb_bg;
        }
#endif
        printf("Easyboot Exception Handler\nException #%02x\n\n", excno);
#ifdef CONSOLE_FB
        fb_bg = 0;
#endif
        __asm__ __volatile__("mrs %0, sp_el0;mov %1, sp" :"=r"(r0),"=r"(r1)::);
        printf("  ESR %016x   ELR %016x\n SPSR %016x   FAR %016x\nSCTLR %016x   TCR %016x\n  SP0 %016x   SP1 %016x\n\n",
            esr, elr, spsr, far, sctlr, tcr, r0, r1);
        printf("Code\n%D\nStack\n%4D\n", elr, r0);
    }
    __asm__ __volatile__("1: wfe; b 1b");
}

/*****************************************
 *      Easyboot loader entry point      *
 *****************************************/
/* actually almost. We come here from _preambule which has relocated us and initialized ARM things for C */
void _start(void)
{
    plg_hdr_t *plg;

    /* initialize everything to zero */
    memset((void*)(uintptr_t)_bss_start, 0, _bss_end - _bss_start);
    fw_init();
    printf((char*)copy);
    /* the emmc driver might print error messages, so call this after we have a console */
    fw_fsinit();
    if(!root_dir) { printf("ERROR: boot partition not found\n"); goto err; }

    /* load configuration */
    fw_loadconfig();
    fw_loadsetup();
    if(ram < 62*1024*1024) { printf("ERROR: unknown amount of RAM\n"); goto err; }
    else if(verbose) printf("Physical RAM %ld Megabytes\n", ram / 1024 / 1024 + 2);

    /* load plugins and look for root partition */
    fw_loadplugins();
    if(rootuuid.Data1 || rootuuid.Data2 || rootuuid.Data3) {
        if(!fw_findroot()) { printf("ERROR: root partition not found\n"); goto err; }
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

    /* finish up things, finalize tags list */
    fw_fini();

    /* transfer control to kernel. Should never return */
    if(verbose) printf("Parsing kernel...\n");
    if(plg) (*((void(* PLG_API)(uint8_t *, uint64_t))((uintptr_t)plg->entry)))(krnl_buf, krnl_size);
    else fw_loadkernel();
    printf("ERROR: unable to start kernel\n");

    /* there's nowhere to return to, halt machine */
err:__asm__ __volatile__("1:wfe; b 1b;");
}
