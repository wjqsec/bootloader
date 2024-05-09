/*
 * src/plugins/obsdboot.c
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
 * @brief Implements the OpenBSD boot protocol
 * https://man.openbsd.org/boot.8
 */

#include "../loader.h"

#ifdef __x86_64__

EASY_PLUGIN(PLG_T_KERNEL) {
    { 0x00, 4, PLG_M_CONST, { 0x7F, 'E', 'L', 'F' } },      /* ELF magic */
    { 0x04, 4, PLG_M_CONST, { 1, 1, 1, 0 } },               /* CLASS32, little-endian, executable */
    { 0x12, 2, PLG_M_CONST, { 3, 0, 0, 0 } },               /* machine i386 */
    /* it would be nice if we could look for OpenBSD ABI, but the image is wrong, has SysV ABI in the ELF header */
    { 0x18, 4, PLG_M_CONST, { 0x20, 1, 4, 0 } },            /* entry point = 0x40120 */
    { 0x120, 2, PLG_M_CONST, { 0x66, 0x58, 0, 0 } },        /* popl %eax */
    { 0x122, 4, PLG_M_CONST, { 0x66, 0x3D, 0x0D, 0xD0 } }   /* cmpl $0xD00D, %eax */
};

PLG_API void _start(uint8_t *buf, uint64_t size)
{
    if(verbose) printf("Starting OpenBSD boot...\n");
    /* bail out if we're running on EFI */
    if(ST) { printf("ERROR: boot is BIOS only\n"); return; }
    /* OpenBSD boot has a fixed address, and a totally non-standard real mode ELF entry point... */
    memcpy((void*)0x40000, buf, size);
    *((uint32_t*)0x7BF8) = (uint32_t)*((uint8_t*)0x4ff);    /* sp + 4: BIOS drive code */
    *((uint32_t*)0x7BF4) = (uint32_t)0xD00D;                /* sp + 0: BOOTMAGIC */
    /* the entry point is hardcoded in the boot sector, and it's set in the segment part of the address */
    if(verbose > 2) printf("Kernel entry:\n%4D", 0x40120);
    if(verbose > 1)
        printf("Transfering real mode control to 4012:0000()\n");
    __asm__ __volatile__(
    /* relocate real mode code to 0x580 */
    ".byte 0xe8;.long 0;"
    "1:popq %%rsi;addq $2f - 1b, %%rsi;"
    "movq $0x580, %%rdi;"
    "movq $3f - 2f, %%rcx;repnz movsb;"
    "movq $16, %%rax;push %%rax;"       /* long -> compat */
    "movq $0x58A, %%rax;push %%rax;"
    "lretq;.code32;2:.word 0x3ff;.quad 0;"
    "lidt (0x580);"                     /* set up IVT */
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
    "xorw %%ax, %%ax;movw %%ax, %%ss;movw $0x7C0, %%ax;movw %%ax, %%ds;movw %%ax, %%es;"
    "movl $0x7BF4, %%esp;movl $0x7BFC, %%ebp;"
    "movw $0x0003, %%ax;int $0x10;"
    "ljmp $0x4012, $0;"                 /* transfer control to boot entry point */
    "3:":::);

}

#endif
