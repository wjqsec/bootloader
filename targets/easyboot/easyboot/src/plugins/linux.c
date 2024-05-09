/*
 * src/plugins/linux.c
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
 * @brief Implements the Linux Boot protocol on AArch64 (RPi) and x86_64
 * https://www.kernel.org/doc/html/latest/arch/x86/boot.html
 */

#include "../loader.h"

#ifdef __aarch64__

EASY_PLUGIN(PLG_T_KERNEL) {
    { 0x00, 2, PLG_M_CONST, { 'M', 'Z', 0, 0 } },
    { 0x38, 4, PLG_M_CONST, { 'A', 'R', 'M', 64 } }
};

PLG_API void _start(uint8_t *buf, uint64_t size)
{
    (void)buf;
    if(verbose) printf("Starting Linux kernel...\n");
    /* copy the kernel where the VideoCore would have loaded it */
    loadseg(0, size, 0x80000, size);
    if(verbose > 1) printf("Transfering control to %08x(%08x)\n", 0x80000, dsdt_ptr);
    /* set up registers and jump to kernel */
    __asm__ __volatile__(
    "mov x0, %0; mov x1, xzr; mov x2, xzr; mov x3, xzr;"
    "mov x30,#0x80000; mov sp, x30; ret"
    ::"r"(dsdt_ptr):);
}
#endif

#ifdef __x86_64__

EASY_PLUGIN(PLG_T_KERNEL) {
    { 0x1fe, 2, PLG_M_CONST, { 0x55, 0xAA, 0, 0 } },
    { 0x202, 4, PLG_M_CONST, { 'H', 'd', 'r', 'S' } }
};

/*** Linux kernel header ***/
#define HDRSMAG "HdrS"
#define E820_MAX_ENTRIES_ZEROPAGE 128
#define LOADED_HIGH   (1<<0)
#define CAN_USE_HEAP  (1<<7)
#define XLF_KERNEL_64 (1<<0)
/* holy fucking shit, these go to orig_video_isVGA... undocumented, I had to grep the kernel source to figure out */
#define VIDEO_TYPE_VLFB 0x23  /* VESA VGA in graphic mode	*/
#define VIDEO_TYPE_EFI  0x70  /* EFI graphic mode */
/* for hdr.vid_mode */
#define VIDEO_MODE_ASK  0xfffd
#define VIDEO_MODE_CUR  0x0f04

typedef struct {
  uint8_t   setup_sects;      /* if 0, WR 4 */
  uint16_t  root_flags;
  uint32_t  syssize;
  uint16_t  ram_size;         /* DO NOT USE */
  uint16_t  vid_mode;         /* WR */
  uint16_t  root_dev;
  uint16_t  boot_flag;        /* 0xAA55 magic */
  /* 0x200 */
  uint16_t  jump;
  uint32_t  header;           /* magic "HdrS" */
  uint16_t  version;          /* boot protocol version, 0x20c minimum */
  uint32_t  realmode_swtch;
  uint16_t  start_sys_seg;    /* obsolete, 0x1000 */
  uint16_t  kernel_version;   /* pointer to kernel version string */
  uint8_t   type_of_loader;   /* WR 0x14 */
  uint8_t   loadflags;        /* WR, bit 1: LOADED_HIGH, bit 7: heap_end_ptr is valid */
  uint16_t  setup_move_size;
  uint32_t  code32_start;
  uint32_t  ramdisk_image;    /* WR, initrd load address (set by boot loader) */
  uint32_t  ramdisk_size;     /* WR, initrd size (set by boot loader) */
  uint32_t  bootsect_klundge; /* DO NOT USE */
  uint16_t  heap_end_ptr;     /* WR */
  uint8_t   ext_loader_ver;
  uint8_t   ext_loader_type;
  uint32_t  cmd_line_ptr;     /* WR */
  uint32_t  initrd_addr_max;
  /* we might not have these, check version */
  uint32_t  kernel_alignment;
  uint8_t   reloc_kernel;
  uint8_t   min_alignment;
  uint16_t  xloadflags;
  uint32_t  cmdline_size;
  uint32_t  hw_subarch;       /* WR 0 */
  uint64_t  hw_subarch_data;  /* WR 0 */
  uint32_t  payload_offset;
  uint32_t  payload_length;
  uint64_t  setup_data;
  uint64_t  pref_address;     /* prefered loading address */
  uint32_t  init_size;
  uint32_t  handover_offset;  /* EFI entry point, obsolete */
  uint32_t  kernel_info_offset; /* v2.15+ WR 0 */
} __attribute__((packed)) linux_boot_t;

typedef struct {
  uint64_t  addr;
  uint64_t  size;
  uint32_t  type;
} __attribute__((packed)) linux_e820_entry_t;

/* The so-called "zeropage" */
typedef struct {
  /* screen info                0x000 */
  uint8_t   orig_x;           /* 0x00 */
  uint8_t   orig_y;           /* 0x01 */
  uint16_t  ext_mem_k;        /* 0x02 */
  uint16_t  orig_video_page;  /* 0x04 */
  uint8_t   orig_video_mode;  /* 0x06 */
  uint8_t   orig_video_cols;  /* 0x07 */
  uint8_t   flags;            /* 0x08 */
  uint8_t   unused2;          /* 0x09 */
  uint16_t  orig_video_ega_bx;/* 0x0a */
  uint16_t  unused3;          /* 0x0c */
  uint8_t   orig_video_lines; /* 0x0e */
  uint8_t   orig_video_isVGA; /* 0x0f */
  uint16_t  orig_video_points;/* 0x10 */
  /* VESA graphic mode -- linear frame buffer */
  uint16_t  lfb_width;        /* 0x12 */
  uint16_t  lfb_height;       /* 0x14 */
  uint16_t  lfb_depth;        /* 0x16 */
  uint32_t  lfb_base;         /* 0x18 */
  uint32_t  lfb_size;         /* 0x1c */
  uint16_t  cl_magic, cl_offset; /* 0x20 */
  uint16_t  lfb_linelength;   /* 0x24 */
  uint8_t   red_size;         /* 0x26 */
  uint8_t   red_pos;          /* 0x27 */
  uint8_t   green_size;       /* 0x28 */
  uint8_t   green_pos;        /* 0x29 */
  uint8_t   blue_size;        /* 0x2a */
  uint8_t   blue_pos;         /* 0x2b */
  uint8_t   rsvd_size;        /* 0x2c */
  uint8_t   rsvd_pos;         /* 0x2d */
  uint16_t  vesapm_seg;       /* 0x2e */
  uint16_t  vesapm_off;       /* 0x30 */
  uint16_t  pages;            /* 0x32 */
  uint16_t  vesa_attributes;  /* 0x34 */
  uint32_t  capabilities;     /* 0x36 */
  uint32_t  ext_lfb_base;     /* 0x3a */
  uint8_t   _reserved[2];     /* 0x3e */
  uint8_t   apm_bios_info[0x054 - 0x040]; /* 0x040 */
  uint8_t   _pad2[4];         /* 0x054 */
  uint64_t  tboot_addr;       /* 0x058 */
  uint8_t   ist_info[0x070 - 0x060]; /* 0x060 */
  uint64_t  acpi_rsdp_addr;   /* 0x070 */
  uint8_t   _pad3[8];         /* 0x078 */
  uint8_t   hd0_info[16]; /* obsolete! */ /* 0x080 */
  uint8_t   hd1_info[16]; /* obsolete! */ /* 0x090 */
  /* sys_desc_table, obsolete    0x0a0 */
  uint16_t  length;
  uint8_t   table[14];
  /* olpc_ofw_header             0x0b0 */
  uint32_t  ofw_magic;
  uint32_t  ofw_version;
  uint32_t  cif_handler;
  uint32_t  irq_desc_table;
  uint32_t  ext_ramdisk_image;/* 0x0c0 */
  uint32_t  ext_ramdisk_size; /* 0x0c4 */
  uint32_t  ext_cmd_line_ptr; /* 0x0c8 */
  uint8_t   _pad4[112];       /* 0x0cc */
  uint32_t  cc_blob_address;  /* 0x13c */
  uint8_t   edid_info[0x1c0 - 0x140]; /* 0x140 */
  /* efi_info                    0x1c0 */
  uint32_t  efi_loader_signature;
  uint32_t  efi_systab;
  uint32_t  efi_memdesc_size;
  uint32_t  efi_memdesc_version;
  uint32_t  efi_memmap;
  uint32_t  efi_memmap_size;
  uint32_t  efi_systab_hi;
  uint32_t  efi_memmap_hi;
  uint32_t  alt_mem_k;        /* 0x1e0 */
  uint32_t  scratch;    /* Scratch field! */  /* 0x1e4 */
  uint8_t   e820_entries;     /* 0x1e8 */
  uint8_t   eddbuf_entries;   /* 0x1e9 */
  uint8_t   edd_mbr_sig_buf_entries; /* 0x1ea */
  uint8_t   kbd_status;       /* 0x1eb */
  uint8_t   secure_boot;      /* 0x1ec */
  uint8_t   _pad5[2];         /* 0x1ed */
  uint8_t   sentinel;         /* 0x1ef */
  uint8_t   _pad6[1];         /* 0x1f0 */
  linux_boot_t hdr;    /* setup header */  /* 0x1f1 */
  uint8_t   _pad7[0x290-0x1f1-sizeof(linux_boot_t)];
  uint8_t   edd_mbr_sig_buffer[0x2d0 - 0x290];  /* 0x290 */
  linux_e820_entry_t e820_table[E820_MAX_ENTRIES_ZEROPAGE]; /* 0x2d0 */
  uint8_t   _pad8[48];        /* 0xcd0 */
  uint8_t   eddbuf[0xeec - 0xd00]; /* 0xd00 */
  uint8_t   _pad9[276];       /* 0xeec */
} __attribute__((packed)) linux_boot_params_t;

PLG_API void _start(uint8_t *buf, uint64_t size)
{
    linux_boot_t *hdr = (linux_boot_t*)(buf + 0x1f1);
    linux_boot_params_t *zero_page = (linux_boot_params_t *)0x90000;
    efi_memory_descriptor_t *mement = (efi_memory_descriptor_t *)0x80000;
    multiboot_mmap_entry_t *memmap = NULL;
    multiboot_tag_framebuffer_t *vidmode;
    multiboot_tag_module_t *initrd;
    uint8_t *ptr, *tag;
    int i, nmm = 0;
    uint64_t off, siz, ent;

    if(hdr->version < 0x20c || ((hdr->pref_address + size) >> 32L)) {
        printf("ERROR: unsupported Linux boot protocol version\n"); return;
    }
    if(verbose) printf("Starting Linux kernel...\n");

    /* copy the kernel */
    if(!hdr->setup_sects) hdr->setup_sects = 4;
    /* unfortunately there are buggy kernels in the wild */
    off = (hdr->setup_sects + 1) * 512;
    siz = hdr->init_size;
    ent = hdr->pref_address + 512;
    if(off + siz >= size) { printf("WARN: out of bound segment\n"); siz = size - off; }
    memset(zero_page, 0, sizeof(linux_boot_params_t));
    memcpy(&zero_page->hdr, hdr, 0x202 - 0x1f1 + buf[0x201]);
    zero_page->acpi_rsdp_addr = (uintptr_t)rsdp_ptr;
    zero_page->hdr.root_dev = 0x100; zero_page->hdr.root_flags = 1; zero_page->hdr.vid_mode = 0xffff;
    zero_page->hdr.type_of_loader = 0xff;
    /*zero_page->hdr.type_of_loader = 0xe0; zero_page->hdr.ext_loader_type = 0x14;*/

    if(!loadseg(off, siz, hdr->pref_address, siz)) return;

    /* iterate on MBI and convert tags to zero_page entries */
    for(tag = tags_buf + sizeof(multiboot_info_t);
      ((multiboot_tag_t*)tag)->type != MULTIBOOT_TAG_TYPE_END && ((multiboot_tag_t*)tag)->size;
      tag = (uint8_t*)(((uintptr_t)tag + ((multiboot_tag_t*)tag)->size + 7) & ~7))
        switch(((multiboot_tag_t*)tag)->type) {
            case MULTIBOOT_TAG_TYPE_CMDLINE:
                ptr = (uint8_t*)zero_page + sizeof(linux_boot_params_t);
                zero_page->hdr.cmd_line_ptr = (uint32_t)(uintptr_t)ptr;
                for(i = 0; i < 32767 && i < (int)((multiboot_tag_t*)tag)->size - 8; i++) ptr[i] = tag[i + 8];
                ptr[i] = 0;
            break;
            case MULTIBOOT_TAG_TYPE_MMAP:
                nmm = (((multiboot_tag_t*)tag)->size - 16) / sizeof(multiboot_mmap_entry_t);
                memmap = ((multiboot_tag_mmap_t*)tag)->entries;
                for(i = 0; i < nmm && i < E820_MAX_ENTRIES_ZEROPAGE; i++) {
                    zero_page->e820_table[i].addr = memmap[i].base_addr;
                    zero_page->e820_table[i].size = memmap[i].length;
                    zero_page->e820_table[i].type = memmap[i].type;
                }
                zero_page->e820_entries = i;
            break;
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                vidmode = (multiboot_tag_framebuffer_t*)tag;
                zero_page->lfb_width = vidmode->framebuffer_width;
                zero_page->lfb_height = vidmode->framebuffer_height;
                zero_page->lfb_depth = vidmode->framebuffer_bpp;
                zero_page->lfb_base = vidmode->framebuffer_addr;
                zero_page->lfb_size = (vidmode->framebuffer_pitch * vidmode->framebuffer_height * vidmode->framebuffer_bpp) >> 3;
                zero_page->lfb_linelength = vidmode->framebuffer_pitch;
                zero_page->red_size = vidmode->framebuffer_red_mask_size;
                zero_page->red_pos = vidmode->framebuffer_red_field_position;
                zero_page->green_size = vidmode->framebuffer_green_mask_size;
                zero_page->green_pos = vidmode->framebuffer_green_field_position;
                zero_page->blue_size = vidmode->framebuffer_blue_mask_size;
                zero_page->blue_pos = vidmode->framebuffer_blue_field_position;
                zero_page->orig_video_isVGA = zero_page->efi_systab ? VIDEO_TYPE_EFI : VIDEO_TYPE_VLFB;
                zero_page->hdr.vid_mode = VIDEO_MODE_CUR;
            break;
            case MULTIBOOT_TAG_TYPE_EFI64:
                memcpy(&zero_page->efi_loader_signature, "EL64", 4);
                zero_page->efi_systab = (uint32_t)(uintptr_t)((multiboot_tag_efi64_t*)tag)->pointer;
                zero_page->efi_systab_hi = (uint32_t)((uintptr_t)((multiboot_tag_efi64_t*)tag)->pointer >> 32L);
                if(zero_page->orig_video_isVGA == VIDEO_TYPE_VLFB)
                    zero_page->orig_video_isVGA = VIDEO_TYPE_EFI;
                if(memmap && nmm > 0) {
                    memset(mement, 0, 65536);
                    zero_page->efi_memmap = (uint32_t)(uintptr_t)mement;
                    zero_page->efi_memmap_size = nmm * sizeof(efi_memory_descriptor_t);
                    zero_page->efi_memdesc_size = sizeof(efi_memory_descriptor_t);
                    zero_page->efi_memdesc_version = 0;
                    for(i = 0; i < nmm && i < 65536/(int)sizeof(efi_memory_descriptor_t); i++) {
                        mement[i].PhysicalStart = memmap[i].base_addr;
                        mement[i].NumberOfPages = memmap[i].length >> 12;
                        mement[i].Type = memmap[i].reserved;
                    }
                }
            break;
            case MULTIBOOT_TAG_TYPE_MODULE:
                if(!zero_page->hdr.ramdisk_image) {
                    initrd = (multiboot_tag_module_t*)tag;
                    zero_page->hdr.ramdisk_image = initrd->mod_start;
                    zero_page->hdr.ramdisk_size = initrd->mod_end - initrd->mod_start;
                }
            break;
            case MULTIBOOT_TAG_TYPE_EDID:
                memcpy(&zero_page->edid_info, tag + 8,
                  ((multiboot_tag_t*)tag)->size > 136 ? 128 : ((multiboot_tag_t*)tag)->size - 8);
            break;
        }
    if(verbose > 1) printf("Transfering control to %08x(%08x)\n", ent, zero_page);
    /* set up registers and jump to kernel */
    __asm__ __volatile__(
    "jmp *%%rax"
    ::"S"(zero_page),"a"(ent):);
}
#endif
