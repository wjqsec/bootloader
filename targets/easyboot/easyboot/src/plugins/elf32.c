/*
 * src/plugins/elf32.c
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
 * @brief Implements Multiboot2 protocol for kernels in ELF32 format
 * https://www.man7.org/linux/man-pages/man5/elf.5.html
 */

#include "../loader.h"

#ifdef __x86_64__

EASY_PLUGIN(PLG_T_KERNEL) {
    { 0x00, 4, PLG_M_CONST, { 0x7F, 'E', 'L', 'F' } },  /* ELF magic */
    { 0x04, 1, PLG_M_CONST, { 1, 0, 0, 0 } },           /* CLASS32 */
    { 0x12, 2, PLG_M_CONST, { 3, 0, 0, 0 } }            /* machine i386 */
};

PLG_API void _start(uint8_t *buf, uint64_t size)
{
    uint8_t *ptr, *ent = NULL;
    int i;

    if(verbose) printf("Starting Multiboot2 ELF32 kernel...\n");
    ent = (uint8_t*)(uintptr_t)((Elf32_Ehdr*)buf)->e_entry;
    ptr = buf + ((Elf32_Ehdr*)buf)->e_phoff;
    for(i = 0; i < ((Elf32_Ehdr*)buf)->e_phnum && ptr + ((Elf32_Ehdr*)buf)->e_phentsize < buf + size;
      i++, ptr += ((Elf32_Ehdr*)buf)->e_phentsize)
        if(((Elf32_Phdr*)ptr)->p_type == PT_LOAD && !loadseg(
            (((Elf32_Phdr*)ptr)->p_offset), (((Elf32_Phdr*)ptr)->p_filesz),
            (((Elf32_Phdr*)ptr)->p_vaddr), (((Elf32_Phdr*)ptr)->p_memsz))) return;
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
