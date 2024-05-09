/*
 * src/plugins/menuet.c
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
 * @brief Implements the MenuetOS boot protocol
 * https://menuetos.net/
 */

#include "../loader.h"

#ifdef __x86_64__

EASY_PLUGIN(PLG_T_KERNEL) {
    { 0x03, 4, PLG_M_CONST, { 'M', 'e', 'n', 'u' } },
    { 0x07, 2, PLG_M_CONST, { 'e', 't', 0, 0 } }
};

typedef struct {
  uint8_t   magic[8];
  uint32_t  pm_entry1;      /* 0x700 */
  uint32_t  pm_entry2;      /* 0x704 */
  uint32_t  boot_info_base; /* 0x708 */
  uint32_t  kernel_base;    /* 0x70c */
  uint32_t  config_base;    /* 0x710 */
  uint32_t  ramdisk_base;   /* 0x714 */
  uint32_t  lgdt_base;      /* 0x718 */
  /* only in memory */
  uint32_t  config_size;    /* 0x71c */
  uint32_t  config_ptr;     /* 0x720 */
  uint32_t  ramdisk_size;   /* 0x724 */
  uint32_t  ramdisk_ptr;    /* 0x728 */
} pmentry0_t;

typedef struct {
  uint64_t  bpp;            /* mandatory */
  uint64_t  vesa_mode;
  uint64_t  mouse_port;
  uint64_t  lfb;            /* mandatory */
  uint64_t  x_res;          /* mandatory */
  uint64_t  y_res;          /* mandatory */
  uint64_t  memory_model;
  uint64_t  mtrr;
  uint64_t  pitch;          /* mandatory */
  uint64_t  bytespp;        /* mandatory */
  uint64_t  mouse_packet;
  uint64_t  gfx_driver;
  uint64_t  gfx_numpixel;
  uint64_t  auto_config;    /* mandatory */
  uint64_t  memory_map_ptr; /* mandatory */
  uint64_t  num_memory_map; /* mandatory */
  uint64_t  rdsp_ptr;
} boot_info_t;

PLG_API void _start(uint8_t *buf, uint64_t size)
{
    pmentry0_t *pm = (pmentry0_t*)0x6f8;
    boot_info_t *boot_info = (boot_info_t*)0x90000;
    uint8_t *tag, *s;
    uint32_t n;

    if(verbose) printf("Starting MenuetOS %c%c kernel...\n", buf[12], buf[13]);

    /* set defaults */
    memset(pm, 0, sizeof(pmentry0_t));
    pm->boot_info_base = 0x9000;
    pm->kernel_base = 0x10000;
    pm->config_base = 0x54000;
    pm->ramdisk_base = 0x100000;
    /* find PMENTRY0 header */
    for(n = 0; n < 4096 && *((uint64_t*)(buf + n)) != 0x305952544e454d50; n += 8);
    if(n < 4096) memcpy(&pm->pm_entry1, buf + n + 8, (uintptr_t)&pm->config_size - (uintptr_t)&pm->pm_entry1);
    if(pm->boot_info_base < 0x1000 || pm->kernel_base < 0x1000 || pm->config_base < 0x30000 || pm->config_base >= 0x8F000 ||
      pm->ramdisk_base < 0x100000) { printf("ERROR: unsupported MenuetOS configuration\n"); return; }

    /* convert boot data and find CONFIG.MNT and RAMDISK.MNT */
    memset(boot_info, 0, sizeof(boot_info_t));
    boot_info->auto_config = 1;
    for(tag = tags_buf + sizeof(multiboot_info_t);
      ((multiboot_tag_t*)tag)->type != MULTIBOOT_TAG_TYPE_END && ((multiboot_tag_t*)tag)->size;
      tag = (uint8_t*)(((uintptr_t)tag + ((multiboot_tag_t*)tag)->size + 7) & ~7))
        switch(((multiboot_tag_t*)tag)->type) {
            case MULTIBOOT_TAG_TYPE_ACPI_OLD: boot_info->rdsp_ptr = (uintptr_t)&((multiboot_tag_old_acpi_t *)tag)->rsdp[0]; break;
            case MULTIBOOT_TAG_TYPE_ACPI_NEW: boot_info->rdsp_ptr = (uintptr_t)&((multiboot_tag_new_acpi_t *)tag)->rsdp[0]; break;
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                boot_info->bpp = ((multiboot_tag_framebuffer_t*)tag)->framebuffer_bpp;
                boot_info->pitch = ((multiboot_tag_framebuffer_t*)tag)->framebuffer_pitch;
                boot_info->x_res = ((multiboot_tag_framebuffer_t*)tag)->framebuffer_width;
                boot_info->y_res = ((multiboot_tag_framebuffer_t*)tag)->framebuffer_height;
                boot_info->lfb = ((multiboot_tag_framebuffer_t*)tag)->framebuffer_addr;
                boot_info->bytespp = (boot_info->bpp + 7) >> 3;
                boot_info->gfx_driver = !!ST;
            break;
            case MULTIBOOT_TAG_TYPE_MODULE:
                s = (uint8_t*)(uintptr_t)((multiboot_tag_module_t*)tag)->mod_start;
                if(s[0] == 0xEB && !memcmp(s + 3, "MENUETOS", 8)) {
                    pm->ramdisk_ptr = ((multiboot_tag_module_t*)tag)->mod_start;
                    pm->ramdisk_size = ((multiboot_tag_module_t*)tag)->mod_end - ((multiboot_tag_module_t*)tag)->mod_start + 1;
                    if(verbose) printf("Detected ramdisk (%d bytes)...\n", (uint64_t)pm->ramdisk_size);
                } else
                if(!pm->config_ptr) {
                    pm->config_ptr = ((multiboot_tag_module_t*)tag)->mod_start;
                    pm->config_size = ((multiboot_tag_module_t*)tag)->mod_end - ((multiboot_tag_module_t*)tag)->mod_start + 1;
                    if(verbose) printf("Detected config (%d bytes)...\n", (uint64_t)pm->config_size);
                }
            break;
            case MULTIBOOT_TAG_TYPE_MMAP:
                n = ((multiboot_tag_mmap_t*)tag)->size - 16;
                if(n > 0xa000 - sizeof(boot_info_t)) n = 0xa000 - sizeof(boot_info_t);
                boot_info->num_memory_map = n / ((multiboot_tag_mmap_t*)tag)->entry_size;
                boot_info->memory_map_ptr = 0x90000 + sizeof(boot_info_t);
                memcpy((void*)(0x90000 + sizeof(boot_info_t)), tag + 16, n);
            break;
            case MULTIBOOT_TAG_TYPE_CMDLINE:
                for(s = tag + sizeof(multiboot_tag_t); *s; s++) {
                    while(*s == ' ') s++;
                    if(!memcmp(s, "noauto", 6)) { boot_info->auto_config = 0; } else
                    if(!memcmp(s, "real", 4)) { pm->pm_entry1 = 0; }
                    while(*s && *s != ' ') s++;
                }
            break;
        }

    if(pm->pm_entry1 && pm->lgdt_base) {
        /* we have a protected mode entry point, use it, this works on BIOS and UEFI as well */
        if(verbose > 1) printf("Transfering prot mode control to %08x(%x)\n", pm->pm_entry1, pm->config_base);
        __asm__ __volatile__(
        /* relocate prot mode code to 0x580 */
        ".byte 0xe8;.long 0;"
        "1:popq %%rsi;addq $2f - 1b, %%rsi;"
        "movq %%rcx, %%rbp;"
        "movq $0x580, %%rdi;"
        "movq $16, %%rax;push %%rax;"       /* long -> compat */
        "push %%rdi;"
        "movq $3f - 2f, %%rcx;repnz movsb;"
        "lretq;.code32;2:;"
        "movl %%cr0, %%eax;"                /* disable CR0.PG */
        "btrl $31, %%eax;btrl $8, %%eax;"
        "movl %%eax, %%cr0;"
        "movl %%cr4, %%eax;"                /* disable PAE */
        "andb $0xDF, %%al;"
        "movl %%eax, %%cr4;"
        "movl %%edx, %%edi;"
        "movl $0x0C0000080, %%ecx;"         /* disable long in EFER MSR */
        "rdmsr;btcl $8, %%eax;wrmsr;"       /* on 32-bit, this messes with %edx */
        /* this is where KERNEL.MNT wants to be (we can't copy this sooner as it conflicts with our loader) */
        "movl %%edi, %%ecx;movl (0x70c), %%edi;movl %%ebx, %%esi;repnz movsb;"
        /* copy boot_info */
        "movl (0x708), %%edi;movl %%edi, %%esp;movl $0x90000, %%esi;movl $64, %%ecx;repnz movsd;"
        /* copy config */
        "movl (0x710), %%edi;movl (0x720), %%esi;orl %%esi, %%esi;jz 1f;movl (0x71c), %%ecx;repnz movsb;1:;xorb %%al,%%al;stosb;"
        /* copy ramdisk */
        "movl (0x714), %%edi;movl (0x728), %%esi;orl %%esi, %%esi;jz 1f;movl (0x724), %%ecx;repnz movsb;1:;xorb %%al,%%al;stosb;"
        "movl (0x718), %%eax;lgdt (%%eax);" /* set up new GDT */
        "jmpl *%%ebp;"                      /* transfer control to KERNEL.MNT entry point */
        "3:"::"b"(buf),"d"(size),"c"(pm->pm_entry1):);
    } else {
        /* fallback when we only know the real mode entry point, this is BIOS only */
        if(ST) { printf("ERROR: old MenuetOS kernel, it is BIOS only\n"); return; }
        if(verbose > 1) printf("Transfering real mode control to 1000:0000(54000)\n");
        __asm__ __volatile__(
        /* relocate real mode code to 0x580 */
        ".byte 0xe8;.long 0;"
        "1:popq %%rsi;addq $2f - 1b, %%rsi;"
        "movq $0x580, %%rdi;"
        "movq $3f - 2f, %%rcx;repnz movsb;"
        "movq $16, %%rax;push %%rax;"       /* long -> compat */
        "movq $0x580+4f-2f, %%rax;push %%rax;"
        "lretq;.code32;2:.word 0x3ff;.quad 0;.long 0;.word 0;"
        ".long 0x0000FFFF;.long 0x00009800;"/*   8 - legacy real cs */
        "4:lidt (0x580);"                   /* set up IVT */
        "movl $0x588, (0x512); lgdt (0x510);"
        "movl %%cr0, %%eax;"                /* disable CR0.PG */
        "btcl $31, %%eax;"
        "movl %%eax, %%cr0;"
        "pushl %%edx;"
        "movl $0x0C0000080, %%ecx;"         /* disable long in EFER MSR */
        "rdmsr;btcl $8, %%eax;wrmsr;"       /* on 32-bit, this messes with %edx */
        "popl %%ecx;"
        /* this is where the boot sector loads KERNEL.MNT (we can't copy this sooner as it conflicts with our loader) */
        "movl $0x10000, %%edi;movl %%ebx, %%esi;repnz movsb;"
        /* copy config */
        "movl $0x54000, %%edi;movl (0x720), %%esi;orl %%esi, %%esi;jz 1f;movl (0x71c), %%ecx;repnz movsb;1:;xorb %%al,%%al;stosb;"
        "ljmp $8,$0x580+1f-2b;1:;"          /* compat -> legacy prot */
        "movl %%cr0, %%eax;"                /* disable CR0.PR */
        "andb $0xfe, %%al;"
        "movl %%eax, %%cr0;"
        "ljmp $0,$0x580+1f-2b;.code16;1:;"  /* prot -> real */
        "xorw %%ax, %%ax;movw %%ax, %%ss;movw %%ax, %%ds;movw %%ax, %%es;"
        "movl $0x7C00, %%esp;movl %%esp, %%ebp;"
        "movw $0x0003, %%ax;int $0x10;"     /* set VGA teletype mode and clear screen */
        "ljmp $0x1000, $0;"                 /* transfer control to KERNEL.MNT entry point */
        "3:"::"b"(buf),"d"(size):);
    }
}

#endif
