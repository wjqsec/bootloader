/*
 * src/plugins/fdos.c
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
 * @brief Implements the FreeDOS boot protocol
 * https://www.freedos.org/
 */

#include "../loader.h"

#ifdef __x86_64__

EASY_PLUGIN(PLG_T_KERNEL) {
    { 0x00, 4, PLG_M_CONST, { 0xEB, 0x1B, 'C', 'O' } },
    { 0x04, 4, PLG_M_CONST, { 'N', 'F', 'I', 'G' } }
};

PLG_API void _start(uint8_t *buf, uint64_t size)
{
    if(verbose) printf("Starting FreeDOS kernel...\n");
    /* bail out if we're running on EFI */
    if(ST) { printf("ERROR: FreeDOS is BIOS only\n"); return; }
    if(verbose > 1) printf("Transfering real mode control to 0060:0000()\n");
    /* NOTE: this overwrites our loader. Luckily the plugin with this asm block is loaded above 1M. */
    __asm__ __volatile__(
    /* relocate real mode code to 0x560 */
    ".byte 0xe8;.long 0;"
    "1:popq %%rsi;addq $2f - 1b, %%rsi;"
    "xorq %%rdi, %%rdi;movw $0x560, %%di;"
    "xorq %%rcx, %%rcx;movb $3f - 2f, %%cl;repnz movsb;"
    "xorq %%rax, %%rax;movb $16, %%al;push %%rax;"/* long -> compat */
    "movw $0x560+4f-2f, %%ax;push %%rax;"
    "lretq;.code32;2:.word 0x3ff;.long 0;.quad 0;.word 0;"
    ".long 0x0000FFFF;.long 0x00009800;"/*   8 - legacy real cs */
    "4:lidt (0x560);"                   /* set up IVT */
    "movl $0x568, (0x512); lgdt (0x510);"
    "movl %%cr0, %%eax;"                /* disable CR0.PG */
    "btcl $31, %%eax;"
    "movl %%eax, %%cr0;"
    "pushl %%edx;"
    "movl $0x0C0000080, %%ecx;"         /* disable long in EFER MSR */
    "rdmsr;btcl $8, %%eax;wrmsr;"       /* on 32-bit, this messes with %edx */
    "popl %%ecx;"
    /* this is where the boot sector loads KERNEL.SYS (we can't copy this sooner as it conflicts with the paging tables) */
    "movw $0x600, %%di;movl %%ebx, %%esi;repnz movsb;"
    "xorl %%ebx, %%ebx;movb (0x4ff), %%bl;"
    /* overwrite kernel config, InitKernelConfig.ForceLBA = 1 and InitKernelConfig.GlobalEnableLBAsupport = 1 */
    "movw $0x0101, (0x60d);"
    "ljmp $8,$0x560+1f-2b;1:;"          /* compat -> legacy prot */
    "movl %%cr0, %%eax;"                /* disable CR0.PR */
    "andb $0xfe, %%al;"
    "movl %%eax, %%cr0;"
    "ljmp $0,$0x560+1f-2b;.code16;1:;"  /* prot -> real */
    "movw $0x1fe0, %%ax;movw %%ax, %%ds;movw %%ax, %%es;movw %%ax, %%ss;"
    "movl $0x7c00, %%ebp;movl %%ebp, %%esp;"
    "pushw %%bx;"
    "movw $0x0003, %%ax;int $0x10;"     /* disable VBE */
    /* make sure the most significant 16 bits in registers are zerod out. Also BIOS drive code in BL and DL */
    "popw %%bx;movl %%ebx, %%edx;xorl %%esi, %%esi;xorl %%edi, %%edi;"
    "ljmp $0x60,$0;"                    /* transfer control to KERNEL.SYS */
    "3:"::"b"(buf),"d"(size):);
}

#endif
