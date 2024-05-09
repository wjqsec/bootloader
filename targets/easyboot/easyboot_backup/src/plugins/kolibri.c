/*
 * src/plugins/kolibri.c
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
 * @brief Implements the KolibriOS boot protocol
 * https://kolibrios.org/en/
 */

#include "../loader.h"

#ifdef __x86_64__

EASY_PLUGIN(PLG_T_KERNEL) {
    { 0x03, 4, PLG_M_CONST, { 'K', 'o', 'l', 'i' } },
    { 0x07, 3, PLG_M_CONST, { 'b', 'r', 'i', 0 } }
};

#define MAX_MEMMAP_BLOCKS   32

typedef struct {
  uint64_t addr;
  uint64_t size;
  uint32_t type;
} __attribute__((packed)) e820entry;

typedef struct {
  uint8_t   bpp;
  uint16_t  pitch;
  uint8_t   pad0[5];
  uint16_t  vesa_mode;
  uint16_t  x_res;
  uint16_t  y_res;
  uint8_t   pad1[6];
  uint32_t  bank_switch;
  uint32_t  lfb;
  uint8_t   mtrr;
  uint8_t   launcher_start;
  uint8_t   debug_print;
  uint8_t   dma;
  uint8_t   pci_access_mechanism;
  uint8_t   pci_last_bus;
  uint16_t  pci_version;
  uint32_t  pci_pm_entry;
  uint8_t   pad2[8];
  uint8_t   shutdown_type;
  uint8_t   pad3[15];
  uint32_t  apm_entry;
  uint16_t  apm_version;
  uint16_t  apm_flags;
  uint8_t   pad4[8];
  uint16_t  apm_code_32;
  uint16_t  apm_code_16;
  uint16_t  apm_data_16;
  uint16_t  rd_read_from;
  uint16_t  kernel_restart;
  uint16_t  sys_disk;
  uint32_t  acpi_rdsp;
  uint8_t   syspath[0x17];
  uint32_t  devicedat_data;
  uint32_t  devicedat_size;
  uint8_t   bios_hd_cnt;
  uint8_t   bios_hd[0x80];
  uint32_t  memmap_block_cnt;
  e820entry memmap_block[MAX_MEMMAP_BLOCKS];
} __attribute__((packed)) BOOT_LO;

PLG_API void _start(uint8_t *buf, uint64_t size)
{
    BOOT_LO *boot_lo = (BOOT_LO*)0x90000; /* copy to 0x9000 */
    uint32_t ent = 0, i;
    uint8_t *tag, *ptr, *s, *d;

    if(verbose) printf("Starting Kolibri kernel...\n");

    /* convert boot data and find specific modules (KOLIBRI.IMG, DEVICES.DAT) */
    *((uint64_t*)0x550) = 0;
    memset(boot_lo, 0, sizeof(BOOT_LO));
    boot_lo->debug_print = verbose > 1;
    boot_lo->launcher_start = 1;
    boot_lo->dma = 2;
    boot_lo->pci_access_mechanism = 1;
    boot_lo->pci_last_bus = 254;
    boot_lo->pci_version = 0x0300;
    boot_lo->rd_read_from = 5;
    for(tag = tags_buf + sizeof(multiboot_info_t);
      ((multiboot_tag_t*)tag)->type != MULTIBOOT_TAG_TYPE_END && ((multiboot_tag_t*)tag)->size;
      tag = (uint8_t*)(((uintptr_t)tag + ((multiboot_tag_t*)tag)->size + 7) & ~7))
        switch(((multiboot_tag_t*)tag)->type) {
            case MULTIBOOT_TAG_TYPE_ACPI_OLD: boot_lo->acpi_rdsp = (uint32_t)(uintptr_t)&((multiboot_tag_old_acpi_t *)tag)->rsdp[0]; break;
            case MULTIBOOT_TAG_TYPE_ACPI_NEW: boot_lo->acpi_rdsp = (uint32_t)(uintptr_t)&((multiboot_tag_new_acpi_t *)tag)->rsdp[0]; break;
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                boot_lo->bpp = ((multiboot_tag_framebuffer_t*)tag)->framebuffer_bpp;
                boot_lo->pitch = ((multiboot_tag_framebuffer_t*)tag)->framebuffer_pitch;
                boot_lo->x_res = ((multiboot_tag_framebuffer_t*)tag)->framebuffer_width;
                boot_lo->y_res = ((multiboot_tag_framebuffer_t*)tag)->framebuffer_height;
                boot_lo->lfb = ((multiboot_tag_framebuffer_t*)tag)->framebuffer_addr;
            break;
            case MULTIBOOT_TAG_TYPE_CMDLINE:
                /* use the same config options on command line like KOLIBRI.INI does */
                for(s = tag + sizeof(multiboot_tag_t); *s; s++) {
                    while(*s == ' ') s++;
                    if(!memcmp(s, "dma=", 4)) { if(s[4] == '1' || s[4] == 'n') boot_lo->dma = 1; } else
                    if(!memcmp(s, "launcher_start=", 15)) { boot_lo->launcher_start = s[15] - '0'; } else
                    if(!memcmp(s, "mtrr=", 5)) { boot_lo->mtrr = s[5] - '0'; } else
                    if(!memcmp(s, "imgfrom=", 8)) { boot_lo->rd_read_from = s[8] - '0'; } else
                    if(!memcmp(s, "shutdown_type=", 14)) { boot_lo->shutdown_type = s[14] - '0'; } else
                    if(!memcmp(s, "kernel_restart=", 15)) { boot_lo->kernel_restart = s[15] - '0'; } else
                    if(!memcmp(s, "sysdisk=", 8)) {
                        boot_lo->sys_disk = (s[9] << 8) | s[8];
                        if(!boot_lo->syspath[0])
                            switch(s[8]) {
                                case 'a': case 'b': case 'c': case 'd':
                                    memcpy(boot_lo->syspath, "/hd0/0/", 8);
                                    boot_lo->syspath[3] += s[8] - 'a'; boot_lo->syspath[5] = s[9];
                                break;
                                case 'r': memcpy(boot_lo->syspath, "/rd/1/", 7); boot_lo->syspath[4] = s[9] == '0' ? '1' : s[9]; break;
                                case 'm': memcpy(boot_lo->syspath, "/m0/", 5); break;
                                default: boot_lo->syspath[0] = 0; break;
                            }
                    } else
                    if(!memcmp(s, "syspath=", 8)) {
                        for(s += 8, d = boot_lo->syspath; *s && *s != ' ' && d < &boot_lo->syspath[0x16]; s++, d++) *d = *s;
                        *d = 0;
                        if(!boot_lo->sys_disk) {
                            if(!memcmp(boot_lo->syspath, "/hd", 3))
                                boot_lo->sys_disk = (boot_lo->syspath[5] << 8) | (boot_lo->syspath[3] - '0' + 'a');
                            else
                            if(!memcmp(boot_lo->syspath, "/rd", 3)) boot_lo->sys_disk = (boot_lo->syspath[5] << 8) | 'r'; else
                            if(!memcmp(boot_lo->syspath, "/m", 2)) boot_lo->sys_disk = ('0' << 8) | 'm'; else
                            if(boot_lo->rd_read_from == 3) boot_lo->sys_disk = ('1' << 8) | 'r';
                        }
                    }
                    while(*s && *s != ' ') s++;
                }
            break;
            case MULTIBOOT_TAG_TYPE_MODULE:
                ptr = (uint8_t*)(uintptr_t)((multiboot_tag_module_t*)tag)->mod_start;
                if(ptr[0] == 0xEB && !memcmp(ptr + 3, "KOLIBRI", 7)) {
                    *((uint32_t*)0x550) = ((multiboot_tag_module_t*)tag)->mod_start;
                    *((uint32_t*)0x554) = ((multiboot_tag_module_t*)tag)->mod_end - ((multiboot_tag_module_t*)tag)->mod_start;
                    boot_lo->rd_read_from = 3;
                    if(!boot_lo->syspath[0]) { memcpy(boot_lo->syspath, "/rd/1/", 7); boot_lo->sys_disk = 'r' | ('1' << 8); }
                    if(verbose) printf("Detected ramdisk (%d bytes)...\n", *((uint32_t*)0x554));
                } else
                if(!boot_lo->devicedat_data) {
                    boot_lo->devicedat_data = ((multiboot_tag_module_t*)tag)->mod_start;
                    boot_lo->devicedat_size = ((multiboot_tag_module_t*)tag)->mod_end - ((multiboot_tag_module_t*)tag)->mod_start;
                    if(verbose) printf("Detected devices data (%d bytes)...\n", boot_lo->devicedat_size);
                }
            break;
            case MULTIBOOT_TAG_TYPE_MMAP:
                boot_lo->memmap_block_cnt = (((multiboot_tag_mmap_t*)tag)->size - 16) / ((multiboot_tag_mmap_t*)tag)->entry_size;
                if(boot_lo->memmap_block_cnt > MAX_MEMMAP_BLOCKS) boot_lo->memmap_block_cnt = MAX_MEMMAP_BLOCKS;
                for(ptr = tag + 16, i = 0; i < boot_lo->memmap_block_cnt; ptr += ((multiboot_tag_mmap_t*)tag)->entry_size, i++) {
                    boot_lo->memmap_block[i].addr = ((multiboot_mmap_entry_t*)ptr)->base_addr;
                    boot_lo->memmap_block[i].size = ((multiboot_mmap_entry_t*)ptr)->length;
                    boot_lo->memmap_block[i].type = ((multiboot_mmap_entry_t*)ptr)->type;
                }
            break;
        }

    /* find the protected mode entry point; for newer kernels it's easy, recorded in the header */
    ent = *((uint32_t*)(buf + 0x20));
    if(ent <= 0x1800 || ent >= 0x3000) {
        /* but for older kernels, the header has something like 0x20202020 here, so do a fallback for those */
        /* look for "mov $0x10, %ax; mov %ax, %ds; mov %ax, %es" at dword aligned address, most likely 0x1A78 */
        for(ent = 0x1800; ent < 0x3000 && *((uint64_t*)(buf + ent)) != 0xC08ED88E0010B866; ent += 4);
    }

    if(ent > 0x1800 && ent < 0x3000) {
        /* we have a protected mode entry point, use it, this works on BIOS and UEFI as well */
        ent += 0x10000;
        if(verbose > 1)
            printf("Transfering prot mode control to %08x()\n", ent);
        __asm__ __volatile__(
        /* relocate prot mode code to 0x580 */
        ".byte 0xe8;.long 0;"
        "1:popq %%rsi;addq $2f - 1b, %%rsi;"
        "movq %%rcx, %%rbp;"
        "movq $0x580, %%rdi;"
        "movq $16, %%rax;push %%rax;"       /* long -> compat */
        "movq $0x580+4f-2f, %%rax;push %%rax;"
        "movq $3f - 2f, %%rcx;repnz movsb;"
        "lretq;.code32;2:;.quad 0;"
        ".long 0x0000FFFF;.long 0x00CF9A00;"/*   8 - 32-bit code */
        ".long 0x0000FFFF;.long 0x00CF9200;"/*  16 - flat data */
        ".long 0x0000FFFF;.long 0x00AF9A00;"/*  24 - 64-bit code */
        "4:movl %%cr0, %%eax;"              /* disable CR0.PG */
        "btrl $31, %%eax;btrl $8, %%eax;"
        "movl %%eax, %%cr0;"
        "movl %%cr4, %%eax;"                /* disable PAE */
        "andb $0xDF, %%al;"
        "movl %%eax, %%cr4;"
        "movl %%edx, %%edi;"
        "movl $0x0C0000080, %%ecx;"         /* disable long in EFER MSR */
        "rdmsr;btcl $8, %%eax;wrmsr;"       /* on 32-bit, this messes with %edx */
        /* this is where the boot sector loads KERNEL.MNT (we can't copy this sooner as it conflicts with our loader) */
        "movl %%edi, %%ecx;movl $0x10000, %%edi;movl %%ebx, %%esi;repnz movsb;"
        /* copy BOOT_LO */
        "movl $0x9000, %%edi;movl $0x90000, %%esi;movl $4096, %%ecx;repnz movsb;"
        /* copy ramdisk */
        "movl (0x550), %%esi;orl %%esi, %%esi;jz 1f;movl (0x554), %%ecx;movl $0x100000, %%edi;repnz movsb;1:;"
        /* set up new GDT */
        "movw $0x1F, (0x510);movl $0x580, (0x512);lgdt (0x510);ljmp $8,$0x580+1f-2b;1:;"
        "jmpl *%%ebp;"                      /* transfer control to KERNEL.MNT entry point */
        "3:"::"b"(buf),"d"(size),"c"(ent):);
    } else {
        /* fallback when we only know the real mode entry point, this is BIOS only */
        if(ST) { printf("ERROR: cannot boot in real mode (use uefi4kos.efi)\n"); return; }
        if(verbose > 1) printf("Transfering real mode control to 1000:0000()\n");
        __asm__ __volatile__(
        /* relocate real mode code to 0x580 */
        ".byte 0xe8;.long 0;"
        "1:popq %%rsi;addq $2f - 1b, %%rsi;"
        "movq $0x580, %%rdi;"
        "movq $3f - 2f, %%rcx;repnz movsb;"
        "movq $16, %%rax;push %%rax;"       /* long -> compat */
        "movq $0x580+4f-2f, %%rax;push %%rax;"
        "lretq;.code32;2:.word 0x3ff;.long 0;.quad 0;.word 0;"
        ".long 0x0000FFFF;.long 0x00009800;"/*   8 - legacy real cs */
        "5:.quad 1;"                        /* loader block */
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
        /* copy ramdisk */
        "movl (0x550), %%esi;orl %%esi, %%esi;jz 1f;movl (0x554), %%ecx;movl $0x100000, %%edi;repnz movsb;1:;"
        "ljmp $8,$0x580+1f-2b;1:;"          /* compat -> legacy prot */
        "movl %%cr0, %%eax;"                /* disable CR0.PR */
        "andb $0xfe, %%al;"
        "movl %%eax, %%cr0;"
        "ljmp $0,$0x580+1f-2b;.code16;1:;"  /* prot -> real */
        "xorw %%ax, %%ax;movw %%ax, %%ss;movw %%ax, %%ds;movw %%ax, %%es;"
        "movl $0x7C00, %%esp;movl %%esp, %%ebp;"
        "movw $0x0003, %%ax;int $0x10;"
        "movw $0x4c4b, %%ax; movw $0x580+5b-2b, %%si;"
        "ljmp $0x1000, $0;"                 /* transfer control to KERNEL.MNT entry point */
        "3:"::"b"(buf),"d"(size):);
    }
}

#endif
