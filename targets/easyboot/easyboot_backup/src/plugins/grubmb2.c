/*
 * src/plugins/grubmb2.c
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
 * @brief Implements GRUB-compatible Multiboot2 protocol, which:
 * - requires special embedded data in kernel, the OS image tags
 * - x86 only (we don't support mips32 the only other option)
 * - 32-bit kernel entry point (64-bit not supported by GRUB)
 * - non-standard ABI (arg1 eax, arg2 ebx, no stack)
 * https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html
 */

#include "../loader.h"

#ifdef __x86_64__

EASY_PLUGIN(PLG_T_KERNEL) {
    { 0x08, 4, PLG_M_SRCH, { 0xD6, 0x50, 0x52, 0xE8 } }
};

PLG_API void _start(uint8_t *buf, uint64_t size)
{
    uint32_t ent = 0, offs = 0, filesz = 0, vaddr = 0, memsz = 0;
    uint8_t *ptr, *tag, *end, bad = 0;
    int i;

    if(verbose) printf("Loading 32-bit Multiboot2 kernel...\n");
    /* spec says that address fields might be zero if kernel is in ELF format */
    if(!memcmp(((Elf32_Ehdr*)buf)->e_ident, ELFMAG, 4) && ((Elf32_Ehdr*)buf)->e_ident[EI_CLASS] == ELFCLASS32) {
        ent = ((Elf32_Ehdr*)buf)->e_entry;
        ptr = buf + ((Elf32_Ehdr*)buf)->e_phoff;
        for(i = 0; i < ((Elf32_Ehdr*)buf)->e_phnum && ptr + ((Elf32_Ehdr*)buf)->e_phentsize < buf + size;
          i++, ptr += ((Elf32_Ehdr*)buf)->e_phentsize)
            if(((Elf32_Phdr*)ptr)->p_type == PT_LOAD && !loadseg(
                (((Elf32_Phdr*)ptr)->p_offset), (((Elf32_Phdr*)ptr)->p_filesz),
                (((Elf32_Phdr*)ptr)->p_vaddr), (((Elf32_Phdr*)ptr)->p_memsz))) return;
    } else {
        /* find 3.1 OS image format Multiboot2 header */
        for(ptr = buf; ptr < buf + 32768 && *((uint32_t*)ptr) != 0xE85250D6; ptr += 8);
        /* iterate on tags */
        for(tag = ptr, end = ptr + *((uint32_t*)(ptr + 8)), ptr += 16; ptr < end && *((uint16_t*)ptr) && *((uint16_t*)(ptr + 4));
          ptr += (*((uint16_t*)(ptr + 4)) + 7) & ~7)
            switch(*((uint16_t*)ptr)) {
                case 2: /* 3.1.5 The address tag of Multiboot2 header */
                    /* Jesus f*cking Christ, why not, oh why not a simple file relative offset and length is used??? */
                    if(*((uint32_t*)(ptr + 12)) == 0xffffffff) {
                        offs = 0; vaddr = *((uint32_t*)(ptr + 8)) - (uint32_t)(tag - buf);
                    } else {
                        vaddr = *((uint32_t*)(ptr + 12)); offs = (uint32_t)(tag - buf) - (*((uint32_t*)(ptr + 8)) - vaddr);
                    }
                    filesz = *((uint32_t*)(ptr + 16)) ? *((uint32_t*)(ptr + 16)) - vaddr : size - offs;
                    memsz = *((uint32_t*)(ptr + 20)) ? *((uint32_t*)(ptr + 20)) - vaddr : filesz;
                break;
                case 3: /* 3.1.6 The entry address tag of Multiboot2 header */ ent = *((uint32_t*)(ptr + 8)); break;
                /* there's nothing we can do about any other tag, either because they are badly specified or it's too late for them */
                case 7: /* 3.1.12 EFI boot services tag - too late, ExitBootServices was already called */
                case 8: /* 3.1.7 EFI i386 entry address tag of Multiboot2 header - not compatible with BIOS nor with UEFI64 */
                case 9: /* 3.1.8 EFI amd64 entry address tag of Multiboot2 header - too late, ExitBootServices was already called */
                    bad = 1; break;
                /* case 1: 3.1.4 Multiboot2 information request - plain stupid and meaningless
                 * case 4: 3.1.9 Flags tag - useless, obsolete EGA/VGA text modes not supported by modern hardware
                 * case 5: 3.1.10 The framebuffer tag of Multiboot2 header - too late for that, and we provide a framebuffer anyway
                 * case 6: 3.1.11 Module alignment tag - plain stupid. Spec expects modules to be page aligned even without this...
                 * case 10: 3.1.13 Relocatable header tag - plain stupid, and unusable without knowing the image relative entry point */
            }
        if(bad || !vaddr || !ent || !filesz || !memsz || offs + filesz > size) { printf("ERROR: invalid Multiboot2 header\n"); return; }
        if(!loadseg(offs, filesz, vaddr, memsz)) return;
    }
    if(verbose > 2) printf("Kernel entry:\n%4D", ent);
    if(verbose > 1)
        printf("Transfering prot mode control to %04x(%04x, %04x)\n", ent, MULTIBOOT2_BOOTLOADER_MAGIC, tags_buf);
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
    "movl %%edi, %%eax;"
    "jmpl *%%ebp;"                      /* transfer control to kernel */
    "3:"::"c"(ent),"d"(MULTIBOOT2_BOOTLOADER_MAGIC),"b"(tags_buf):);
}

#endif
