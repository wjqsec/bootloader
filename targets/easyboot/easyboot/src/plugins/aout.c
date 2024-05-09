/*
 * src/plugins/aout.c
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
 * @brief Implements Multiboot2 protocol for kernels in a.out format
 * https://man.cat-v.org/unix_8th/5/a.out
 */

#include "../loader.h"

#ifdef __x86_64__

EASY_PLUGIN(PLG_T_KERNEL) {
    { 0x00, 4, PLG_M_CONST, { 0x08, 0x01, 0, 0 } }   /* a.out magic (NMAGIC, pure executable) */
};

struct exec
{
    uint32_t   a_midmag;
    uint32_t   a_text;
    uint32_t   a_data;
    uint32_t   a_bss;
    uint32_t   a_syms;
    uint32_t   a_entry;
    uint32_t   a_trsize;
    uint32_t   a_drsize;
};

PLG_API void _start(uint8_t *buf, uint64_t size)
{
    uint8_t *ent = NULL;

    (void)size;
    if(verbose) printf("Starting Multiboot2 a.out kernel...\n");
    ent = (uint8_t*)(uintptr_t)((struct exec*)buf)->a_entry;
    if(!loadseg(sizeof(struct exec), ((struct exec*)buf)->a_text + ((struct exec*)buf)->a_data, (uint64_t)ent,
        ((struct exec*)buf)->a_text + ((struct exec*)buf)->a_data + ((struct exec*)buf)->a_bss)) return;
    if(verbose > 2) printf("Kernel entry:\n%4D", ent);
    if(verbose > 1)
        printf("Transfering prot mode control to %08x(%08x, %08x)\n", ent, MULTIBOOT2_BOOTLOADER_MAGIC, tags_buf);
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
    "btcl $31, %%eax;"
    "movl %%eax, %%cr0;"
    "movw $0x18, %%ax;movw %%ax, %%ds;" /* set 32-bit segment selectors */
    "movw %%ax, %%es;movw %%ax, %%ss;"
    "movw %%ax, %%fs;movw %%ax, %%gs;"
    "movl %%edx, %%edi;"
    "movl $0x0C0000080, %%ecx;"         /* disable long in EFER MSR */
    "rdmsr;btcl $8, %%eax;wrmsr;"       /* on 32-bit, this messes with %edx */
    "xorl %%eax, %%eax;lidt (%%eax);"   /* disable IDT */
    /* CDECL uses the stack for arguments, but fastcall uses %ecx, %edx */
    "movl %%edi, %%eax;movl %%eax, %%ecx;"
    "movl %%ebx, %%edx;movl %%ebx, %%esi;"
    "movl $0x90000, %%esp; pushl %%ebx; pushl %%eax;"
    "jmpl *%%ebp;"                      /* transfer control to kernel */
    "3:"::"c"(ent),"d"(MULTIBOOT2_BOOTLOADER_MAGIC),"b"(tags_buf):);
}

#endif
