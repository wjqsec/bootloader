/*
 * src/plugins/grubmb1.c
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
 * @brief Implements the legacy Multiboot 0.6.96 boot protocol
 * https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
 */

#include "../loader.h"

#ifdef __x86_64__

EASY_PLUGIN(PLG_T_KERNEL) {
    { 0x04, 4, PLG_M_SRCH, { 0x02, 0xB0, 0xAD, 0x1B } }
};

#define MULTIBOOT_BOOTLOADER_MAGIC              0x2BADB002
#define MULTIBOOT_INFO_BOOTDEV                  0x00000002
#define MULTIBOOT_INFO_CMDLINE                  0x00000004
#define MULTIBOOT_INFO_MODS                     0x00000008
#define MULTIBOOT_INFO_MEM_MAP                  0x00000040
#define MULTIBOOT_INFO_FRAMEBUFFER_INFO         0x00001000

typedef struct {
  uint32_t magic;
  uint32_t flags;
  uint32_t checksum;
  uint32_t header_addr;
  uint32_t load_addr;
  uint32_t load_end_addr;
  uint32_t bss_end_addr;
  uint32_t entry_addr;
  uint32_t mode_type;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
} multiboot_header_t;

typedef struct {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  uint32_t cmdline;
  uint32_t mods_count;
  uint32_t mods_addr;
  uint32_t elf_num;
  uint32_t elf_size;
  uint32_t elf_addr;
  uint32_t elf_shndx;
  uint32_t mmap_length;
  uint32_t mmap_addr;
  uint32_t drives_length;
  uint32_t drives_addr;
  uint32_t config_table;
  uint32_t boot_loader_name;
  uint32_t apm_table;
  uint32_t vbe_control_info;
  uint32_t vbe_mode_info;
  uint16_t vbe_mode;
  uint16_t vbe_interface_seg;
  uint16_t vbe_interface_off;
  uint16_t vbe_interface_len;
  uint64_t framebuffer_addr;
  uint32_t framebuffer_pitch;
  uint32_t framebuffer_width;
  uint32_t framebuffer_height;
  uint8_t framebuffer_bpp;
  uint8_t framebuffer_type;
  uint8_t framebuffer_red_field_position;
  uint8_t framebuffer_red_mask_size;
  uint8_t framebuffer_green_field_position;
  uint8_t framebuffer_green_mask_size;
  uint8_t framebuffer_blue_field_position;
  uint8_t framebuffer_blue_mask_size;
} multiboot_mbi_t;

typedef struct {
  uint32_t size;
  uint64_t addr;
  uint64_t len;
  uint32_t type;
} __attribute__((packed)) multiboot_memory_map_t;

typedef struct {
  uint32_t mod_start;
  uint32_t mod_end;
  uint32_t cmdline;
  uint32_t pad;
} multiboot_mod_list_t;

PLG_API void _start(uint8_t *buf, uint64_t size)
{
    multiboot_header_t *hdr;
    multiboot_mbi_t *info;
    uint32_t ent = 0, offs = 0, filesz = 0, vaddr = 0, memsz = 0;
    uint8_t *ptr, *tag;
    int i;

    if(verbose) printf("Loading 32-bit Multiboot1 kernel...\n");

    /* find 3.1 OS image format Multiboot1 header */
    for(ptr = buf; ptr < buf + 8192 && *((uint32_t*)ptr) != 0x1BADB002; ptr += 4);
    hdr = (multiboot_header_t*)ptr;
    /* spec says that address fields might be zero if kernel is in ELF format */
    if(!(hdr->flags & 0x10000) && !memcmp(((Elf32_Ehdr*)buf)->e_ident, ELFMAG, 4) &&
      ((Elf32_Ehdr*)buf)->e_ident[EI_CLASS] == ELFCLASS32) {
        ent = ((Elf32_Ehdr*)buf)->e_entry;
        ptr = buf + ((Elf32_Ehdr*)buf)->e_phoff;
        for(i = 0; i < ((Elf32_Ehdr*)buf)->e_phnum && ptr + ((Elf32_Ehdr*)buf)->e_phentsize < buf + size;
          i++, ptr += ((Elf32_Ehdr*)buf)->e_phentsize)
            if(((Elf32_Phdr*)ptr)->p_type == PT_LOAD && !loadseg(
                (((Elf32_Phdr*)ptr)->p_offset), (((Elf32_Phdr*)ptr)->p_filesz),
                (((Elf32_Phdr*)ptr)->p_vaddr), (((Elf32_Phdr*)ptr)->p_memsz))) return;
    } else {
        if(hdr->header_addr && hdr->load_addr) {
            if(hdr->load_addr == 0xffffffff) {
                offs = 0; vaddr = hdr->header_addr - (uint32_t)(ptr - buf);
            } else {
                vaddr = hdr->load_addr; offs = (uint32_t)(ptr - buf) - (hdr->header_addr - vaddr);
            }
            filesz = hdr->load_end_addr ? hdr->load_end_addr - hdr->load_addr : size - offs;
            memsz = hdr->bss_end_addr ? hdr->bss_end_addr - hdr->load_addr : filesz;
            ent = hdr->entry_addr;
        }
        if(!vaddr) { vaddr = 0x100000; filesz = memsz = size; }
        if(!ent || !filesz || !memsz || offs + filesz > size) { printf("ERROR: invalid Multiboot1 header\n"); return; }
        if(!loadseg(offs, filesz, vaddr, memsz)) return;
    }

    /* convert Multiboot2 to Multiboot1 */
    info = (multiboot_mbi_t*)tags_ptr; memset(info, 0, sizeof(multiboot_mbi_t)); tags_ptr += sizeof(multiboot_mbi_t);
    if(!ST) { info->flags = MULTIBOOT_INFO_BOOTDEV; info->boot_device = *((uint8_t*)0x4ff); }
    for(tag = tags_buf + sizeof(multiboot_info_t);
      ((multiboot_tag_t*)tag)->type != MULTIBOOT_TAG_TYPE_END && ((multiboot_tag_t*)tag)->size;
      tag = (uint8_t*)(((uintptr_t)tag + ((multiboot_tag_t*)tag)->size + 7) & ~7))
        switch(((multiboot_tag_t*)tag)->type) {
            case MULTIBOOT_TAG_TYPE_CMDLINE:
                info->flags |= MULTIBOOT_INFO_CMDLINE;
                info->cmdline = (uint32_t)(uintptr_t)tag + 8;
            break;
            case MULTIBOOT_TAG_TYPE_MODULE:
                info->flags |= MULTIBOOT_INFO_MODS;
                if(!info->mods_addr) info->mods_addr = (uint32_t)(uintptr_t)tags_ptr;
                info->mods_count++;
                ((multiboot_mod_list_t*)tags_ptr)->mod_start = ((multiboot_tag_module_t*)tag)->mod_start;
                ((multiboot_mod_list_t*)tags_ptr)->mod_end = ((multiboot_tag_module_t*)tag)->mod_end;
                ((multiboot_mod_list_t*)tags_ptr)->cmdline = (uint32_t)(uintptr_t)tag + 16;
                tags_ptr += sizeof(multiboot_mod_list_t);
            break;
            case MULTIBOOT_TAG_TYPE_MMAP:
                i = (((multiboot_tag_mmap_t*)tag)->size - 16) / ((multiboot_tag_mmap_t*)tag)->entry_size;
                info->flags |= MULTIBOOT_INFO_MEM_MAP;
                info->mmap_addr = (uint32_t)(uintptr_t)tags_ptr + 4;
                info->mmap_length = i * sizeof(multiboot_memory_map_t);
                for(ptr = tag + 16; i; ptr += ((multiboot_tag_mmap_t*)tag)->entry_size, tags_ptr += 16, i--) {
                    ((multiboot_memory_map_t*)tags_ptr)->size = 16;
                    ((multiboot_memory_map_t*)tags_ptr)->addr = ((multiboot_mmap_entry_t*)ptr)->base_addr;
                    ((multiboot_memory_map_t*)tags_ptr)->len  = ((multiboot_mmap_entry_t*)ptr)->length;
                    ((multiboot_memory_map_t*)tags_ptr)->type = ((multiboot_mmap_entry_t*)ptr)->type;
                }
            break;
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                info->flags |= MULTIBOOT_INFO_FRAMEBUFFER_INFO;
                memcpy(&info->framebuffer_addr, tag + 8, 22);
                memcpy(&info->framebuffer_red_field_position, tag + 32, 6);
            break;
        }
    if(verbose > 2) printf("Kernel entry:\n%4D", ent);
    if(verbose > 1)
        printf("Transfering prot mode control to %08x(%08x, %08x)\n", ent, MULTIBOOT_BOOTLOADER_MAGIC, info);
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
    "3:"::"c"(ent),"d"(MULTIBOOT_BOOTLOADER_MAGIC),"b"(info):);
}

#endif
