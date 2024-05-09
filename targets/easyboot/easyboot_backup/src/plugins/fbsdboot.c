/*
 * src/plugins/fbsdboot.c
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
 * @brief Implements the FreeBSD boot protocol
 * https://docs.freebsd.org/en/books/handbook/boot/
 */

#include "../loader.h"

#ifdef __x86_64__

EASY_PLUGIN(PLG_T_KERNEL) {
    { 0x000, 2, PLG_M_CONST, { 0xEB, 0x3C, 0, 0 } },
    { 0x402, 3, PLG_M_CONST, { 'B', 'T', 'X', 0 } }
};

PLG_API void _start(uint8_t *buf, uint64_t size)
{
    if(verbose) printf("Starting FreeBSD boot...\n");
    /* bail out if we're running on EFI */
    if(ST) { printf("ERROR: boot is BIOS only\n"); return; }
    if(verbose > 1) printf("Transfering real mode control to 0000:%04x()\n", 0x9010);
    /* NOTE: this overwrites our loader. Luckily the plugin with this asm block is loaded above 1M. */
    __asm__ __volatile__(
    /* this is where boot1 loads boot (which is a concatenation of boot1+boot2) */
    "movq $0x8C00, %%rdi;repnz movsb;"
    /* copy out BTX client */
    "movq $0x9000, %%rbx;xorq %%rsi, %%rsi;movw 0xa(%%rbx), %%si;addw %%bx, %%si;"
    "movq $0xC000, %%rdi;movq $8192, %%rcx;repnz movsb;"
    /* boot1 relocates itself here, but we're not executing boot1, so we have to do the copy */
    "movq $0x8C00, %%rsi;movq $0x700, %%rdi;movq $512, %%rcx;repnz movsb;"
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
    "movl $0x0C0000080, %%ecx;"         /* disable long in EFER MSR */
    "rdmsr;btcl $8, %%eax;wrmsr;"       /* on 32-bit, this messes with %edx */
    "ljmp $8,$0x580+1f-2b;1:;"          /* compat -> legacy prot */
    "movl %%cr0, %%eax;"                /* disable CR0.PR */
    "andb $0xfe, %%al;"
    "movl %%eax, %%cr0;"
    "ljmp $0,$0x580+1f-2b;.code16;1:;"  /* prot -> real */
    "xorw %%ax, %%ax;movw %%ax, %%ds;movw %%ax, %%es;movw %%ax, %%ss;"
    "movl $0x7C00, %%esp;movl %%esp, %%ebp;"
    "movw $0x0003, %%ax;int $0x10;"
    "ljmp $0,$0x9010;"                  /* transfer control to BTX server */
    "3:"::"S"(buf),"c"(size):);
}

#endif
