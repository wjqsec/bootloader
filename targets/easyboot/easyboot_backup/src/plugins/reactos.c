/*
 * src/plugins/reactos.c
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
 * @brief Implements the ReactOS boot protocol
 * https://reactos.org/
 */

#include "../loader.h"

#ifdef __x86_64__

EASY_PLUGIN(PLG_T_KERNEL) {
    { 0xDF, 4, PLG_M_CONST, { 'F', 'r', 'e', 'e' } },
    { 0xE3, 4, PLG_M_CONST, { 'L', 'o', 'a', 'd' } }
};

PLG_API void _start(uint8_t *buf, uint64_t size)
{
    uint8_t *tag;

    if(verbose) printf("Starting ReactOS boot...\n");
    /* bail out if we're running on EFI */
    if(ST) { printf("ERROR: ReactOS is BIOS only\n"); return; }

    /* get command line */
    *((uint32_t*)0x550) = 0;
    for(tag = tags_buf + sizeof(multiboot_info_t);
      ((multiboot_tag_t*)tag)->type != MULTIBOOT_TAG_TYPE_END && ((multiboot_tag_t*)tag)->size;
      tag = (uint8_t*)(((uintptr_t)tag + ((multiboot_tag_t*)tag)->size + 7) & ~7))
        if(((multiboot_tag_t*)tag)->type == MULTIBOOT_TAG_TYPE_CMDLINE) { *((uint32_t*)0x550) = (uintptr_t)tag + 8; break; }

    if(verbose > 1) printf("Transfering real mode control to 0000:FA00()\n");
    /* NOTE: this overwrites our loader. Luckily the plugin with this asm block is loaded above 1M. */
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
    "4:lidt (0x580);"                   /* set up IVT */
    "movl $0x588, (0x512); lgdt (0x510);"
    "movl %%cr0, %%eax;"                /* disable CR0.PG */
    "btcl $31, %%eax;"
    "movl %%eax, %%cr0;"
    "pushl %%edx;"
    "movl $0x0C0000080, %%ecx;"         /* disable long in EFER MSR */
    "rdmsr;btcl $8, %%eax;wrmsr;"       /* on 32-bit, this messes with %edx */
    "popl %%ecx;"
    /* this is where the boot sector loads FREELDR.SYS (we can't copy this sooner as it conflicts with our loader) */
    "movl $0xF800, %%edi;movl %%ebx, %%esi;repnz movsb;"
    /* copy command line where FreeLoader expects it (overwrites page tables, but we have already turned paging off) */
    "movb $0, (0x7000);movl (0x550), %%esi;orl %%esi, %%esi;jz 1f;movl $0x7000, %%edi;movl $0xfff, %%ecx;repnz movsb;1:;"
    "ljmp $8,$0x580+1f-2b;1:;"          /* compat -> legacy prot */
    "movl %%cr0, %%eax;"                /* disable CR0.PR */
    "andb $0xfe, %%al;"
    "movl %%eax, %%cr0;"
    "ljmp $0,$0x580+1f-2b;.code16;1:;"  /* prot -> real */
    "xorl %%eax, %%eax;movw %%ax, %%ds;movw %%ax, %%es;movw %%ax, %%ss;"
    "movl $0x7c00, %%ebp;movl %%ebp, %%esp;"
    "movw $0x0003, %%ax;int $0x10;"     /* disable VBE */
    "xorl %%edx, %%edx;movb (0x4ff), %%dl;"
    "ljmp $0,$0xFA00;"                  /* transfer control to FREELDR.SYS */
    "3:"::"b"(buf),"d"(size):);
}

#endif
