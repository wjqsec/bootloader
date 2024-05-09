/*
 * src/misc/plgld.c
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
 * @brief Plugin linker
 *
 * Unfortunately modern toolchains can't create a.out struct exec any more,
 * and ELF is bloated like hell (an empty _start which is literally just a
 * RET instruction, comes up with more than 12Kb). Also taking a quick look
 * at this code, you can see it is ridiculuosly overcomplicated, it's not a
 * kind of complexity that any sane person would want at boot time...
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define HOSTED
#include "../loader.h"  /* elf headers */
#include "x86_64.h"     /* disassmeblers */
#include "aarch64.h"
#ifdef __WIN32__
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
WCHAR szFile[PATH_MAX];
#endif
#define ET_REL              1   /* Relocatable file */

/*** ELF Sections ***/

#define SHT_NULL            0   /* Section header table entry unused */
#define SHT_PROGBITS        1   /* Program data */
#define SHT_SYMTAB          2   /* Symbol table */
#define SHT_STRTAB          3   /* String table */
#define SHT_RELA            4   /* Relocation entries with addends */
#define SHT_HASH            5   /* Symbol hash table */
#define SHT_DYNAMIC         6   /* Dynamic linking information */
#define SHT_NOTE            7   /* Notes */
#define SHT_NOBITS          8   /* Program space with no data (bss) */
#define SHT_REL             9   /* Relocation entries, no addends */
#define SHT_SHLIB           10  /* Reserved */
#define SHT_DYNSYM          11  /* Dynamic linker symbol table */
#define SHT_INIT_ARRAY      14  /* Array of constructors */
#define SHT_FINI_ARRAY      15  /* Array of destructors */
#define SHT_PREINIT_ARRAY   16  /* Array of pre-constructors */
#define SHT_GROUP           17  /* Section group */
#define SHT_SYMTAB_SHNDX    18  /* Extended section indices */
#define SHT_RELR            19  /* RELR relative relocations */
#define	SHT_NUM             20  /* Number of defined types.  */

#define SHF_WRITE     (1 << 0)  /* Writable */
#define SHF_ALLOC     (1 << 1)  /* Occupies memory during execution */
#define SHF_EXECINSTR (1 << 2)  /* Executable */
#define SHF_MERGE     (1 << 4)  /* Might be merged */
#define SHF_STRINGS   (1 << 5)  /* Contains nul-terminated strings */

typedef struct
{
  uint32_t  sh_name;        /* Section name (string tbl index) */
  uint32_t  sh_type;        /* Section type */
  uint64_t  sh_flags;       /* Section flags */
  uint64_t  sh_addr;        /* Section virtual addr at execution */
  uint64_t  sh_offset;      /* Section file offset */
  uint64_t  sh_size;        /* Section size in bytes */
  uint32_t  sh_link;        /* Link to another section */
  uint32_t  sh_info;        /* Additional section information */
  uint64_t  sh_addralign;   /* Section alignment */
  uint64_t  sh_entsize;     /* Entry size if section holds table */
} Elf64_Shdr;

#define ELF64_ST_BIND(val)  (((unsigned char) (val)) >> 4)
#define ELF64_ST_TYPE(val)  ((val) & 0xf)
#define ELF64_ST_INFO(bind, type)   (((bind) << 4) + ((type) & 0xf))
#define STB_LOCAL           0   /* Local symbol */
#define STB_GLOBAL          1   /* Global symbol */
#define STB_WEAK            2   /* Weak symbol */
#define STT_NOTYPE          0   /* Symbol type is unspecified */
#define STT_OBJECT          1   /* Symbol is a data object */
#define STT_FUNC            2   /* Symbol is a code object */
#define STT_SECTION         3   /* Symbol associated with a section */
#define STT_FILE            4   /* Symbol's name is file name */
#define STT_COMMON          5   /* Symbol is a common data object */
#define STT_TLS             6   /* Symbol is thread-local data object*/

typedef struct
{
  uint32_t  st_name;        /* Symbol name (string tbl index) */
  uint8_t   st_info;        /* Symbol type and binding */
  uint8_t   st_other;       /* Symbol visibility */
  uint16_t  st_shndx;       /* Section index */
  uint64_t  st_value;       /* Symbol value */
  uint64_t  st_size;        /* Symbol size */
} Elf64_Sym;

#define ELF64_R_SYM(i)      ((i) >> 32)
#define ELF64_R_TYPE(i)     ((i) & 0xffffffff)
#define ELF64_R_INFO(sym,type)  ((((uint64_t) (sym)) << 32) + (type))

/* AMD x86-64 relocations.  */
#define R_X86_64_64		1	/* Direct 64 bit  */
#define R_X86_64_PC32		2	/* PC relative 32 bit signed */
#define R_X86_64_GOT32		3	/* 32 bit GOT entry */
#define R_X86_64_PLT32		4	/* 32 bit PLT address */
#define R_X86_64_COPY		5	/* Copy symbol at runtime */
#define R_X86_64_GLOB_DAT	6	/* Create GOT entry */
#define R_X86_64_JUMP_SLOT	7	/* Create PLT entry */
#define R_X86_64_RELATIVE	8	/* Adjust by program base */
#define R_X86_64_GOTPCREL	9	/* 32 bit signed PC relative
					   offset to GOT */
#define R_X86_64_32		10	/* Direct 32 bit zero extended */
#define R_X86_64_32S		11	/* Direct 32 bit sign extended */
#define R_X86_64_16		12	/* Direct 16 bit zero extended */
#define R_X86_64_PC16		13	/* 16 bit sign extended pc relative */
#define R_X86_64_8		14	/* Direct 8 bit sign extended  */
#define R_X86_64_PC8		15	/* 8 bit sign extended pc relative */
#define R_X86_64_DTPMOD64	16	/* ID of module containing symbol */
#define R_X86_64_DTPOFF64	17	/* Offset in module's TLS block */
#define R_X86_64_TPOFF64	18	/* Offset in initial TLS block */
#define R_X86_64_TLSGD		19	/* 32 bit signed PC relative offset
					   to two GOT entries for GD symbol */
#define R_X86_64_TLSLD		20	/* 32 bit signed PC relative offset
					   to two GOT entries for LD symbol */
#define R_X86_64_DTPOFF32	21	/* Offset in TLS block */
#define R_X86_64_GOTTPOFF	22	/* 32 bit signed PC relative offset
					   to GOT entry for IE symbol */
#define R_X86_64_TPOFF32	23	/* Offset in initial TLS block */
#define R_X86_64_PC64		24	/* PC relative 64 bit */
#define R_X86_64_GOTOFF64	25	/* 64 bit offset to GOT */
#define R_X86_64_GOTPC32	26	/* 32 bit signed pc relative
					   offset to GOT */
#define R_X86_64_GOT64		27	/* 64-bit GOT entry offset */
#define R_X86_64_GOTPCREL64	28	/* 64-bit PC relative offset
					   to GOT entry */
#define R_X86_64_GOTPC64	29	/* 64-bit PC relative offset to GOT */
#define R_X86_64_GOTPLT64	30 	/* like GOT64, says PLT entry needed */
#define R_X86_64_PLTOFF64	31	/* 64-bit GOT relative offset
					   to PLT entry */
#define R_X86_64_SIZE32		32	/* Size of symbol plus 32-bit addend */
#define R_X86_64_SIZE64		33	/* Size of symbol plus 64-bit addend */
#define R_X86_64_GOTPC32_TLSDESC 34	/* GOT offset for TLS descriptor.  */
#define R_X86_64_TLSDESC_CALL   35	/* Marker for call through TLS
					   descriptor.  */
#define R_X86_64_TLSDESC        36	/* TLS descriptor.  */
#define R_X86_64_IRELATIVE	37	/* Adjust indirectly by program base */
#define R_X86_64_RELATIVE64	38	/* 64-bit adjust by program base */
					/* 39 Reserved was R_X86_64_PC32_BND */
					/* 40 Reserved was R_X86_64_PLT32_BND */
#define R_X86_64_GOTPCRELX	41	/* Load from 32 bit signed pc relative
					   offset to GOT entry without REX
					   prefix, relaxable.  */
#define R_X86_64_REX_GOTPCRELX	42	/* Load from 32 bit signed pc relative
					   offset to GOT entry with REX prefix,
					   relaxable.  */

/* LP64 AArch64 relocs.  */
#define R_AARCH64_ABS64         257	/* Direct 64 bit. */
#define R_AARCH64_ABS32         258	/* Direct 32 bit.  */
#define R_AARCH64_ABS16		259	/* Direct 16-bit.  */
#define R_AARCH64_PREL64	260	/* PC-relative 64-bit.	*/
#define R_AARCH64_PREL32	261	/* PC-relative 32-bit.	*/
#define R_AARCH64_PREL16	262	/* PC-relative 16-bit.	*/
#define R_AARCH64_MOVW_UABS_G0	263	/* Dir. MOVZ imm. from bits 15:0.  */
#define R_AARCH64_MOVW_UABS_G0_NC 264	/* Likewise for MOVK; no check.  */
#define R_AARCH64_MOVW_UABS_G1	265	/* Dir. MOVZ imm. from bits 31:16.  */
#define R_AARCH64_MOVW_UABS_G1_NC 266	/* Likewise for MOVK; no check.  */
#define R_AARCH64_MOVW_UABS_G2	267	/* Dir. MOVZ imm. from bits 47:32.  */
#define R_AARCH64_MOVW_UABS_G2_NC 268	/* Likewise for MOVK; no check.  */
#define R_AARCH64_MOVW_UABS_G3	269	/* Dir. MOV{K,Z} imm. from 63:48.  */
#define R_AARCH64_MOVW_SABS_G0	270	/* Dir. MOV{N,Z} imm. from 15:0.  */
#define R_AARCH64_MOVW_SABS_G1	271	/* Dir. MOV{N,Z} imm. from 31:16.  */
#define R_AARCH64_MOVW_SABS_G2	272	/* Dir. MOV{N,Z} imm. from 47:32.  */
#define R_AARCH64_LD_PREL_LO19	273	/* PC-rel. LD imm. from bits 20:2.  */
#define R_AARCH64_ADR_PREL_LO21	274	/* PC-rel. ADR imm. from bits 20:0.  */
#define R_AARCH64_ADR_PREL_PG_HI21 275	/* Page-rel. ADRP imm. from 32:12.  */
#define R_AARCH64_ADR_PREL_PG_HI21_NC 276 /* Likewise; no overflow check.  */
#define R_AARCH64_ADD_ABS_LO12_NC 277	/* Dir. ADD imm. from bits 11:0.  */
#define R_AARCH64_LDST8_ABS_LO12_NC 278	/* Likewise for LD/ST; no check. */
#define R_AARCH64_TSTBR14	279	/* PC-rel. TBZ/TBNZ imm. from 15:2.  */
#define R_AARCH64_CONDBR19	280	/* PC-rel. cond. br. imm. from 20:2. */
#define R_AARCH64_JUMP26	282	/* PC-rel. B imm. from bits 27:2.  */
#define R_AARCH64_CALL26	283	/* Likewise for CALL.  */
#define R_AARCH64_LDST16_ABS_LO12_NC 284 /* Dir. ADD imm. from bits 11:1.  */
#define R_AARCH64_LDST32_ABS_LO12_NC 285 /* Likewise for bits 11:2.  */
#define R_AARCH64_LDST64_ABS_LO12_NC 286 /* Likewise for bits 11:3.  */
#define R_AARCH64_MOVW_PREL_G0	287	/* PC-rel. MOV{N,Z} imm. from 15:0.  */
#define R_AARCH64_MOVW_PREL_G0_NC 288	/* Likewise for MOVK; no check.  */
#define R_AARCH64_MOVW_PREL_G1	289	/* PC-rel. MOV{N,Z} imm. from 31:16. */
#define R_AARCH64_MOVW_PREL_G1_NC 290	/* Likewise for MOVK; no check.  */
#define R_AARCH64_MOVW_PREL_G2	291	/* PC-rel. MOV{N,Z} imm. from 47:32. */
#define R_AARCH64_MOVW_PREL_G2_NC 292	/* Likewise for MOVK; no check.  */
#define R_AARCH64_MOVW_PREL_G3	293	/* PC-rel. MOV{N,Z} imm. from 63:48. */
#define R_AARCH64_LDST128_ABS_LO12_NC 299 /* Dir. ADD imm. from bits 11:4.  */
#define R_AARCH64_MOVW_GOTOFF_G0 300	/* GOT-rel. off. MOV{N,Z} imm. 15:0. */
#define R_AARCH64_MOVW_GOTOFF_G0_NC 301	/* Likewise for MOVK; no check.  */
#define R_AARCH64_MOVW_GOTOFF_G1 302	/* GOT-rel. o. MOV{N,Z} imm. 31:16.  */
#define R_AARCH64_MOVW_GOTOFF_G1_NC 303	/* Likewise for MOVK; no check.  */
#define R_AARCH64_MOVW_GOTOFF_G2 304	/* GOT-rel. o. MOV{N,Z} imm. 47:32.  */
#define R_AARCH64_MOVW_GOTOFF_G2_NC 305	/* Likewise for MOVK; no check.  */
#define R_AARCH64_MOVW_GOTOFF_G3 306	/* GOT-rel. o. MOV{N,Z} imm. 63:48.  */
#define R_AARCH64_GOTREL64	307	/* GOT-relative 64-bit.  */
#define R_AARCH64_GOTREL32	308	/* GOT-relative 32-bit.  */
#define R_AARCH64_GOT_LD_PREL19	309	/* PC-rel. GOT off. load imm. 20:2.  */
#define R_AARCH64_LD64_GOTOFF_LO15 310	/* GOT-rel. off. LD/ST imm. 14:3.  */
#define R_AARCH64_ADR_GOT_PAGE	311	/* P-page-rel. GOT off. ADRP 32:12.  */
#define R_AARCH64_LD64_GOT_LO12_NC 312	/* Dir. GOT off. LD/ST imm. 11:3.  */
#define R_AARCH64_LD64_GOTPAGE_LO15 313	/* GOT-page-rel. GOT off. LD/ST 14:3 */
#define R_AARCH64_COPY         1024	/* Copy symbol at runtime.  */
#define R_AARCH64_GLOB_DAT     1025	/* Create GOT entry.  */
#define R_AARCH64_JUMP_SLOT    1026	/* Create PLT entry.  */
#define R_AARCH64_RELATIVE     1027	/* Adjust by program base.  */

typedef struct
{
  uint64_t  r_offset;       /* Address */
  uint64_t  r_info;         /* Relocation type and symbol index */
} Elf64_Rel;

typedef struct
{
  uint64_t  r_offset;       /* Address */
  uint64_t  r_info;         /* Relocation type and symbol index */
  int64_t   r_addend;       /* Addend */
} Elf64_Rela;

#define SECTION(i)  ((Elf64_Shdr *)((uint8_t *)shdr + i * ehdr->e_shentsize))
enum { SHT_CODE=1, SHT_RODATA, SHT_DATA, SHT_BSS };

/* relocations in ELF are plain stupid. Let's convert them into something saner and more managable */
#define REL_NONE        0
#define REL_Z           1   /* size of symbol */
#define REL_P           2   /* PC-relative */
#define REL_A           4   /* has addend */
#define REL_S           8   /* symbol address */
#define REL_G           16  /* GOT address (probably with symbol index) */
#define REL_BA          REL_A
#define REL_SA          (REL_S|REL_A)
#define REL_SAP         (REL_S|REL_A|REL_P)
#define REL_ZA          (REL_Z|REL_A)
#define REL_GS          (REL_G|REL_S)
#define REL_GSP         (REL_G|REL_S|REL_P)
#define REL_GSA         (REL_G|REL_S|REL_A)
#define REL_GSAP        (REL_G|REL_S|REL_A|REL_P)
#define REL_GA          (REL_G|REL_A)
#define REL_GAP         (REL_G|REL_A|REL_P)
/* arch, type: identify the ELF relocation
 * calc: calculation method
 * sbit: start bit of address
 * ebit: end bit of address
 * imsk: immedate mask index (see PLG_IM_x defines)
 * neg: bit position of "address is negated" flag if negative address has to be negated */
struct { uint16_t arch; uint32_t type; uint8_t calc; uint8_t sbit; uint8_t ebit; uint8_t imsk; uint8_t neg; } rel_types[] = {
    /* x86_64 relocations */
    { EM_X86_64,    0,                              REL_NONE,   0,  0, 0, 0 },
    { EM_X86_64,    R_X86_64_64,                    REL_SA,     0, 63, 0, 0 },
    { EM_X86_64,    R_X86_64_PC32,                  REL_SAP,    0, 31, 0, 0 },
    { EM_X86_64,    R_X86_64_GOT32,                 REL_GA,     0, 31, 0, 0 },
    { EM_X86_64,    R_X86_64_GOT64,                 REL_GA,     0, 63, 0, 0 },
    { EM_X86_64,    R_X86_64_GLOB_DAT,              REL_S,      0, 63, 0, 0 },
    { EM_X86_64,    R_X86_64_JUMP_SLOT,             REL_S,      0, 63, 0, 0 },
    { EM_X86_64,    R_X86_64_RELATIVE,              REL_BA,     0, 63, 0, 0 },
    { EM_X86_64,    R_X86_64_GOTPCREL,              REL_GAP,    0, 31, 0, 0 },
    { EM_X86_64,    R_X86_64_32,                    REL_SA,     0, 31, 0, 0 },
    { EM_X86_64,    R_X86_64_32S,                   REL_SA,     0, 31, 0, 0 },
    { EM_X86_64,    R_X86_64_16,                    REL_SA,     0, 15, 0, 0 },
    { EM_X86_64,    R_X86_64_PC16,                  REL_SAP,    0, 15, 0, 0 },
    { EM_X86_64,    R_X86_64_8,                     REL_SA,     0,  7, 0, 0 },
    { EM_X86_64,    R_X86_64_PC8,                   REL_SAP,    0,  7, 0, 0 },
    { EM_X86_64,    R_X86_64_PC64,                  REL_SAP,    0, 63, 0, 0 },
    { EM_X86_64,    R_X86_64_GOTPC32,               REL_GAP,    0, 31, 0, 0 },
    { EM_X86_64,    R_X86_64_GOTPC64,               REL_GAP,    0, 63, 0, 0 },
    { EM_X86_64,    R_X86_64_GOTPCREL64,            REL_GSAP,   0, 63, 0, 0 },
    { EM_X86_64,    R_X86_64_GOTPCRELX,             REL_GSAP,   0, 31, 0, 0 },
    { EM_X86_64,    R_X86_64_REX_GOTPCRELX,         REL_GSAP,   0, 31, 0, 0 },
    { EM_X86_64,    R_X86_64_SIZE32,                REL_ZA,     0, 31, 0, 0 },
    { EM_X86_64,    R_X86_64_SIZE64,                REL_ZA,     0, 63, 0, 0 },
    /* aarch64 relocations */
    { EM_AARCH64,   0,                              REL_NONE,   0,  0, 0, 0 },
    { EM_AARCH64,   256,                            REL_NONE,   0,  0, 0, 0 }, /* withdrawn, but might occur */
    { EM_AARCH64,   R_AARCH64_ABS64,                REL_SA,     0, 63, 0, 0 },
    { EM_AARCH64,   R_AARCH64_ABS32,                REL_SA,     0, 31, 0, 0 },
    { EM_AARCH64,   R_AARCH64_ABS16,                REL_SA,     0, 15, 0, 0 },
    { EM_AARCH64,   R_AARCH64_PREL64,               REL_SAP,    0, 63, 0, 0 },
    { EM_AARCH64,   R_AARCH64_PREL32,               REL_SAP,    0, 31, 0, 0 },
    { EM_AARCH64,   R_AARCH64_PREL16,               REL_SAP,    0, 15, 0, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_UABS_G0,         REL_SA,     0, 15, 1, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_UABS_G0_NC,      REL_SA,     0, 15, 1, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_UABS_G1,         REL_SA,    16, 31, 1, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_UABS_G1_NC,      REL_SA,    16, 31, 1, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_UABS_G2,         REL_SA,    32, 47, 1, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_UABS_G2_NC,      REL_SA,    32, 47, 1, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_UABS_G3,         REL_SA,    48, 63, 1, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_SABS_G0,         REL_SA,     0, 15, 1, 30 },
    { EM_AARCH64,   R_AARCH64_MOVW_SABS_G1,         REL_SA,    16, 31, 1, 30 },
    { EM_AARCH64,   R_AARCH64_MOVW_SABS_G2,         REL_SA,    32, 47, 1, 30 },
    { EM_AARCH64,   R_AARCH64_LD_PREL_LO19,         REL_SAP,    2, 20, 1, 0 },
    { EM_AARCH64,   R_AARCH64_ADR_PREL_LO21,        REL_SAP,    0, 20, 3, 0 },
    { EM_AARCH64,   R_AARCH64_ADR_PREL_PG_HI21,     REL_SAP,   12, 32, 3, 0 },
    { EM_AARCH64,   R_AARCH64_ADR_PREL_PG_HI21_NC,  REL_SAP,   12, 32, 3, 0 },
    { EM_AARCH64,   R_AARCH64_ADD_ABS_LO12_NC,      REL_SA,     0, 11, 2, 0 },
    { EM_AARCH64,   R_AARCH64_LDST8_ABS_LO12_NC,    REL_SA,     0, 11, 2, 0 },
    { EM_AARCH64,   R_AARCH64_LDST16_ABS_LO12_NC,   REL_SA,     1, 11, 2, 0 },
    { EM_AARCH64,   R_AARCH64_LDST32_ABS_LO12_NC,   REL_SA,     2, 11, 2, 0 },
    { EM_AARCH64,   R_AARCH64_LDST64_ABS_LO12_NC,   REL_SA,     3, 11, 2, 0 },
    { EM_AARCH64,   R_AARCH64_LDST128_ABS_LO12_NC,  REL_SA,     4, 11, 2, 0 },
    { EM_AARCH64,   R_AARCH64_TSTBR14,              REL_SAP,    2, 15, 1, 0 },
    { EM_AARCH64,   R_AARCH64_CONDBR19,             REL_SAP,    2, 20, 1, 0 },
    { EM_AARCH64,   R_AARCH64_JUMP26,               REL_SAP,    2, 27, 0, 0 },
    { EM_AARCH64,   R_AARCH64_CALL26,               REL_SAP,    2, 27, 0, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_PREL_G0,         REL_SAP,    0, 15, 1, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_PREL_G0_NC,      REL_SAP,    0, 15, 1, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_PREL_G1,         REL_SAP,   16, 31, 1, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_PREL_G1_NC,      REL_SAP,   16, 31, 1, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_PREL_G2,         REL_SAP,   32, 47, 1, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_PREL_G2_NC,      REL_SAP,   32, 47, 1, 0 },
    { EM_AARCH64,   R_AARCH64_MOVW_PREL_G3,         REL_SAP,   48, 63, 1, 0 }
};
/* for external symbols we support absolute and PC-relative relocations
 * for internal symbols we reduce the relocation to a simple add BASE by placing the addend into the relocatable part */

/* symbols used by the plugin relocation records, MUST match plg_got_t in loader.h */
char *plg_got[] = {
    "BASE", "verbose", "file_size", "root_buf", "tags_buf", "tags_ptr", "rsdp_ptr", "dsdt_ptr", "ST",
    "memset", "memcpy", "memcmp", "alloc", "free",
    "printf", "pb_init", "pb_draw", "pb_fini",
    "loadsec", "sethooks",
    "open", "read", "close", "loadfile",
    "loadseg"
};
/* immediate masks are architecture specific. But only 0 allowed for x86_64, and besides that we only have one ARM architecture */
uint64_t imasks[4] = { PLG_IM_0, PLG_IM_AARCH64_1, PLG_IM_AARCH64_2, PLG_IM_AARCH64_3 };

plg_hdr_t plugin;
uint8_t sys_fault;
uint64_t dbg_label, buf_reloc;
int64_t read_size;
char *idtypes[] = { "?    ", "const", "byte ", "word ", "dword", "badd ", "wadd ", "dadd ", "srch " };

/**
 * Test if a plugin matches a buffer or not
 */
int check_magic(uint8_t *buf, uint8_t *plg)
{
    plg_hdr_t *hdr = (plg_hdr_t*)plg;
    plg_id_t *ids = (plg_id_t*)(plg + sizeof(plg_hdr_t));
    uint32_t i, j, offs = 0, o = 0;

    if(!buf || !plg || memcmp(plg, PLG_MAGIC, 4) || (!hdr->nid && hdr->type != PLG_T_TAG)) { printf("plgld: bad plugin\r\n"); return 0; }
    printf(" Num: Offset Size Type      Magic             | offs o    val   buf[o]\r\n");
    for(i = 0; i < hdr->nid; i++, ids++) {
        printf(" %3u: %04x   %02x   %02x %s ", (uint32_t)i, ids->offs, ids->size, ids->type,
            ids->type < 9 ? idtypes[ids->type] : "?    ");
        if(ids->size) {
            for(j = 0; j < ids->size; j++) printf(" %02x", ids->magic[j]);
            for(; j < 4; j++) printf("   ");
            printf("  ");
            for(j = 0; j < ids->size; j++) printf("%c", ids->magic[j] > 32 && ids->magic[j] < 127 ? ids->magic[j] : '.');
            for(; j < 4; j++) printf(".");
        } else printf("  %c", ids->type < PLG_M_BADD ? '=' : '+');
        o = ids->offs + offs;
        printf(" | %04x %04x", offs, o);
        switch(ids->type) {
            case PLG_M_BYTE: o = o > 65535 ? 0 : buf[o]; break;
            case PLG_M_WORD: o = o > 65534 ? 0 : *((uint16_t*)(buf + o)); break;
            case PLG_M_DWORD: o = o > 65532 ? 0 : *((uint32_t*)(buf + o)); break;
            case PLG_M_BADD: o = (o > 65535 ? 0 : buf[o]) + offs; break;
            case PLG_M_WADD: o = (o > 65534 ? 0 : *((uint16_t*)(buf + o))) + offs; break;
            case PLG_M_DADD: o = (o > 65532 ? 0 : *((uint32_t*)(buf + o))) + offs; break;
        }
        printf(" %04x  %02x %02x %02x %02x\r\n", o, buf[o], buf[o+1], buf[o+2], buf[o+3]);
        if(!ids->size) offs = o;
        else if(ids->type == PLG_M_SRCH) {
            for(o = offs; o + ids->size < 65536; o += (ids->offs ? ids->offs : 1))
                if(!memcmp(buf + o, ids->magic, ids->size)) { offs = o; break; }
            if(o + ids->size >= 65536) { i = 256; break; }
        } else if(o + ids->size > 65535 || memcmp(buf + o, ids->magic, ids->size)) { i = 256; break; }
    }
    if(i != hdr->nid) { printf("plgld: no match, not the plugin you're looking for\r\n"); return 0; }
    printf("plgld: %04x OK\r\n", o); return 1;
}

/**
 * Dump a plugin readelf / objdump style
 */
void plg_dump(uint8_t *buf)
{
    plg_hdr_t *hdr = (plg_hdr_t*)buf;
    plg_id_t *ids = (plg_id_t*)(buf + sizeof(plg_hdr_t));
    plg_rel_t *r, *rels = (plg_rel_t*)((uint8_t*)ids + hdr->nid * sizeof(plg_id_t));
    uint8_t *code = (uint8_t*)rels + hdr->nrel * sizeof(plg_rel_t);
    uint8_t *data = code + hdr->codesz, s;
    uint64_t i, j, old, pos;
    char str[256];

    printf("\r\nEasyboot Plugin Header (%x - %x):\r\n", 0, (int)sizeof(plg_hdr_t) - 1);
    if(read_size < (int64_t)(sizeof(plg_hdr_t) + sizeof(plg_id_t) + sizeof(plg_rel_t))) return;
    i = sizeof(plg_hdr_t) + hdr->nid * sizeof(plg_id_t) + hdr->nrel * sizeof(plg_rel_t);
    printf("  Magic:                    %02x %02x %02x %02x\r\n", hdr->magic[0], hdr->magic[1], hdr->magic[2], hdr->magic[3]);
    printf("  File size:                %d%s\r\n", hdr->filesz, hdr->filesz != read_size || i + hdr->codesz + hdr->rosz > hdr->filesz? " (invalid)" : "");
    printf("  In memory size:           %d%s\r\n", hdr->memsz, hdr->filesz > hdr->memsz ? " (invalid)" : "");
    printf("  Code size:                %d%s\r\n", hdr->codesz, hdr->codesz < 1 ? " (invalid)" : "");
    printf("  Read-only data size:      %d\r\n", hdr->rosz);
    printf("  Entry point address:      %06x%s\r\n", hdr->entry, hdr->entry < (uint32_t)(code - buf) || hdr->entry >= code - buf + hdr->codesz ? " (invalid)" : "");
    printf("  Machine:                  %d %s\r\n", hdr->arch,
        hdr->arch == EM_X86_64 ? "x86_64" : (hdr->arch == EM_AARCH64 ? "aarch64" : "?"));
    printf("  Number of relocations:    %d%s\r\n", hdr->nrel, hdr->nrel < 1 ? " (invalid)" : "");
    printf("  Number of identifiers:    %d%s\r\n", hdr->nid, hdr->nid < 1 && hdr->type != PLG_T_TAG ? " (invalid)" : "");
    printf("  Highest GOT entry:        %d\r\n", hdr->ngot);
    printf("  Format revision:          %d\r\n", hdr->rev);
    printf("  Plugin type:              %d %s\r\n", hdr->type,
        hdr->type == 1 ? "file system driver" : (hdr->type == 2 ? "kernel parser" : (hdr->type == 3 ? "decompressor" :
        (hdr->type == 4 ? "tag extender" : "? (invalid)"))));

    printf("\r\nIdentifier match records (%x - %x):\r\n Num: Offset Size Type      Magic\r\n",
        (uint32_t)((uint8_t*)ids - buf), (uint32_t)((uint8_t*)ids - buf + hdr->nid * sizeof(plg_id_t) - 1));
    for(i = 0; i < hdr->nid; i++, ids++) {
        printf(" %3u: %04x   %02x   %02x %s ", (uint32_t)i, ids->offs, ids->size, ids->type,
            ids->type < 9 ? idtypes[ids->type] : "?    ");
        if(ids->size) {
            for(j = 0; j < ids->size; j++) printf(" %02x", ids->magic[j]);
            for(; j < 4; j++) printf("   ");
            printf("  ");
            for(j = 0; j < ids->size; j++) printf("%c", ids->magic[j] > 32 && ids->magic[j] < 127 ? ids->magic[j] : '.');
            for(; j < 4; j++) printf(".");
        } else printf("  %c", ids->type < PLG_M_BADD ? '=' : '+');
        printf("%s\r\n", ids->type < PLG_M_CONST || ids->type > PLG_M_SRCH ? " (invalid)" : "");
    }

    printf("\r\nRelocation records (%x - %x):\r\n Num: Offset Type     ImmMask    Sbit Ebit Neg Sym Address\r\n",
        (uint32_t)((uint8_t*)rels - buf), (uint32_t)((uint8_t*)rels - buf + hdr->nrel * sizeof(plg_rel_t) - 1));
    for(i = 0, r = rels; i < hdr->nrel; i++, r++) {
        s = PLG_R_SYM(r->type); j = PLG_R_IMSK(r->type);
        printf(" %3u: %06x %08x %u %08x   %2u   %2u  %2u %3u %s%s%s%s[%u:%u]%s\r\n", (uint32_t)i, r->offs, r->type, (uint32_t)j,
            (uint32_t)(j < sizeof(imasks)/sizeof(imasks[0]) ? imasks[j] : 0),
            PLG_R_SBIT(r->type), PLG_R_EBIT(r->type), PLG_R_NBIT(r->type),
            s, r->type & PLG_A_PCREL ? "(" : "", s < sizeof(plg_got)/sizeof(plg_got[0]) ? plg_got[(uint32_t)s] : "?",
            r->type & PLG_A_GOTREL ? "@GOT" : "", r->type & PLG_A_PCREL ? " - PC)" : "", PLG_R_SBIT(r->type), PLG_R_EBIT(r->type),
            r->offs < (uint32_t)(code - buf) || r->offs >= hdr->filesz || j >= sizeof(imasks)/sizeof(imasks[0]) ||
            s >= sizeof(plg_got)/sizeof(plg_got[0]) ? " (invalid)" : "");
    }

    printf("\r\nCode section (%x - %x):\r\n", (uint32_t)(code - buf), (uint32_t)(code - buf + hdr->codesz - 1));
    if(hdr->arch != EM_X86_64 && hdr->arch != EM_AARCH64) printf("  (unknown instruction set, no disassembly)\r\n");
    sys_fault = 0; buf_reloc = (uint64_t)(uintptr_t)buf;
    for(pos = (uint64_t)(uintptr_t)code; pos < (uint64_t)(uintptr_t)code + hdr->codesz;) {
        old = pos;
        if(hdr->arch == EM_X86_64 || hdr->arch == EM_AARCH64) {
            if(hdr->arch == EM_X86_64) pos = disasm_x86(pos, str); else pos = disasm_aa64(pos, str);
            if(old == pos) pos++;
        } else {
            pos += 16; if(pos > (uint64_t)(uintptr_t)code + hdr->codesz) pos = (uint64_t)(uintptr_t)code + hdr->codesz;
            str[0] = 0;
        }
        if((uint64_t)(uintptr_t)buf + hdr->entry == old) printf("  <_start>:\r\n"); else
        if((uint64_t)(uintptr_t)buf + hdr->entry > old && (uint64_t)(uintptr_t)buf + hdr->entry < pos)
            printf("  %06x: <_start>:\r\n", hdr->entry);
        printf("  %06x:", (uint32_t)(old - (uint64_t)(uintptr_t)buf));
        for(i = 0; i < pos - old; i++) printf(" %02x", buf[old - (uint64_t)(uintptr_t)buf + i]);
        for(; i < 17; i++) printf("   ");
        printf("%s\r\n", str);
        for(i = 0, r = rels; i < hdr->nrel; i++, r++)
            if(r->offs >= old - (uint64_t)(uintptr_t)buf && r->offs < pos - (uint64_t)(uintptr_t)buf) {
                s = PLG_R_SYM(r->type);
                printf("          <%06x: reloc %d %s%s%s>\r\n", r->offs, (uint32_t)i,
                    s < sizeof(plg_got)/sizeof(plg_got[0]) ? plg_got[(uint32_t)s] : "?",
                    r->type & PLG_A_GOTREL ? "@GOT" : "", r->type & PLG_A_PCREL ? " - PC" : "");
            }
    }

    if(hdr->rosz) {
        printf("\r\nRead only data section (%x - %x):\r\n", (uint32_t)(data - buf), (uint32_t)(data - buf + hdr->rosz - 1));
        for(i = 0, old = (uint32_t)(data - buf); i < hdr->rosz; i += pos, old += pos) {
            pos = hdr->rosz - i > 16 ? 16 : hdr->rosz - i;
            printf("  %06x:", (uint32_t)(data - buf + i));
            for(j = 0; j < pos; j++) printf(" %02x", data[i + j]);
            for(;j < 17; j++) printf("   ");
            for(j = 0; j < pos; j++) printf("%c", data[i + j] >= 32 && data[i + j] < 127 ? data[i + j] : '.');
            printf("\r\n");
            for(j = 0, r = rels; j < hdr->nrel; j++, r++)
                if(r->offs >= old && r->offs < old + pos) {
                    s = PLG_R_SYM(r->type);
                    printf("          <%06x: reloc %d %s%s%s>\r\n", r->offs, (uint32_t)j,
                        s < sizeof(plg_got)/sizeof(plg_got[0]) ? plg_got[(uint32_t)s] : "?",
                        r->type & PLG_A_GOTREL ? "@GOT" : "", r->type & PLG_A_PCREL ? " - PC" : "");
                }
        }
        data += hdr->rosz;
    }

    if((uint32_t)(data - buf) < hdr->filesz) {
        printf("\r\nData section (%x - %x):\r\n", (uint32_t)(data - buf), hdr->filesz - 1);
        for(i = 0, old = hdr->filesz - (data - buf); i < old; i += pos) {
            pos = old - i > 16 ? 16 : old - i;
            printf("  %06x:", (uint32_t)(data - buf + i));
            for(j = 0; j < pos; j++) printf(" %02x", data[i + j]);
            for(;j < 17; j++) printf("   ");
            for(j = 0; j < pos; j++) printf("%c", data[i + j] >= 32 && data[i + j] < 127 ? data[i + j] : '.');
            printf("\r\n");
        }
    }

    if(hdr->memsz > hdr->filesz) {
        printf("\r\nBSS section (%x - %x):\r\n  (not stored in file, all zeros)\r\n",
            hdr->filesz, hdr->memsz - 1);
    }
    printf("\r\n");
}

/**
 * Read a file entirely into memory
 */
unsigned char* readfileall(char *file)
{
#ifdef __WIN32__
    HANDLE f;
    DWORD r, t;
    int i;
#else
    FILE *f;
#endif
    unsigned char *data = NULL;

    read_size = 0;
    if(!file || !*file) return NULL;
#ifdef __WIN32__
    memset(&szFile, 0, sizeof(szFile));
    MultiByteToWideChar(CP_UTF8, 0, file, -1, szFile, PATH_MAX);
    for(i = 0; szFile[i]; i++) if(szFile[i] == L'/') szFile[i] = L'\\';
    f = CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if(f != INVALID_HANDLE_VALUE) {
        r = GetFileSize(f, NULL);
        read_size = (int64_t)r;
        data = (unsigned char*)malloc(read_size + 1);
        if(!data) { fprintf(stderr, "plgld: memory allocation error\r\n"); exit(1); }
        memset(data, 0, read_size + 1);
        if(!ReadFile(f, data, r, &t, NULL)) t = 0;
        read_size = (int64_t)t;
        CloseHandle(f);
    }
#else
    f = fopen(file, "rb");
    if(f) {
        fseek(f, 0L, SEEK_END);
        read_size = (int64_t)ftell(f);
        fseek(f, 0L, SEEK_SET);
        data = (unsigned char*)malloc(read_size + 1);
        if(!data) { fprintf(stderr, "plgld: memory allocation error\r\n"); exit(1); }
        memset(data, 0, read_size + 1);
        read_size = (int64_t)fread(data, 1, read_size, f);
        fclose(f);
    }
#endif
    if(!read_size && data) { free(data); data = NULL; }
    return data;
}

/**
 * Write a memory buffer entirely into file
 */
int writefileall(char *file, uint8_t *data, uint32_t size)
{
#ifdef __WIN32__
    HANDLE f;
    DWORD r, t = 0;
#else
    FILE *f;
#endif
    uint32_t i;

    if(!file || !*file || !data || size < 1) return 0;
#ifdef __WIN32__
    memset(&szFile, 0, sizeof(szFile));
    MultiByteToWideChar(CP_UTF8, 0, file, -1, szFile, PATH_MAX);
    for(i = 0; szFile[i]; i++) if(szFile[i] == L'/') szFile[i] = L'\\';
    r = size;
    f = CreateFileW(szFile, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(f != INVALID_HANDLE_VALUE) {
        if(!WriteFile((HANDLE)f, data, r, &t, NULL) || r != t) t = 0;
        CloseHandle(f);
        return r == t;
    }
#else
    f = fopen(file, "wb");
    if(f) {
        i = (uint32_t)fwrite(data, 1, size, f);
        fclose(f);
        return i == size;
    }
#endif
    fprintf(stderr, "plgld: unable to write '%s'\r\n", file);
    return 0;
}

/**
 * Main function
 */
int main(int argc, char **argv)
{
    char *str;
    uint32_t i, j, k, l, n, *ptr, code, hdrsz;
    uint8_t *data, *out, symb;
    int64_t addr, S, A, P;
    uint64_t m;
    Elf64_Ehdr *ehdr;
    Elf64_Shdr *shdr, *syms = NULL, *strs = NULL, *s, *s2;
    Elf64_Rela *rel;
    Elf64_Sym *sym;
    plg_id_t *match = NULL;
    plg_rel_t *rr;

    /* TODO: allow multiple input files to be linked together */

    /* command line arguments */
    if(argc < 2) {
usage:  printf("Easyboot Plugin Linker, Copyright (c) 2023 bzt, GPLv3+\r\nhttps://gitlab.com/bztsrc/easyboot\r\n\r\n");
        printf(" plgld <in elf> <out plg>\r\n");
        printf(" plgld <in plg>\r\n");
        return 1;
    }
    if(!(data = readfileall(argv[1])) || read_size < (int64_t)sizeof(Elf64_Ehdr)) {
        fprintf(stderr, "plgld: unable to read '%s'\r\n", argv[1]);
        return 1;
    }

    /* if there's only one argument and that's a plugin, dump it */
    if(!memcmp(data, PLG_MAGIC, 4)) {
        if(argc == 3) { out = readfileall(argv[2]); check_magic(out, data); free(out); }
        else plg_dump(data);
        free(data); return 0;
    } else if(argc != 3) goto usage;

    /* initialize output */
    memset(&plugin, 0, sizeof(plugin));
    memcpy(&plugin.magic, PLG_MAGIC, 4);

    /* check input */
    ehdr = (Elf64_Ehdr *)data;
    if(memcmp(ehdr->e_ident, ELFMAG, 4) || ehdr->e_ident[EI_CLASS] != ELFCLASS64 || ehdr->e_ident[EI_DATA] != ELFDATA2LSB ||
      ehdr->e_type != ET_REL) {
        fprintf(stderr, "plgld: not a 64-bit relocatable ELF object '%s'\r\n", argv[1]);
        free(data); return 1;
    }
    plugin.arch = ehdr->e_machine;
    shdr = (Elf64_Shdr *)(data + ehdr->e_shoff);

    /* iterate on ELF sections and collect what we need */
    /* unfortunately we need the symbol table in advance, so we must iterate twice... */
    for(i = 0; i < ehdr->e_shnum; i++) {
        s = SECTION(i);
        if(!s->sh_size) continue;
        if(s->sh_type == SHT_STRTAB) { if(!strs) strs = s; } else
        if(s->sh_type == SHT_SYMTAB) { if(!syms) syms = s; }
    }
    if(!syms || !syms->sh_offset || !strs || !strs->sh_offset) {
        fprintf(stderr, "plgld: no symbols in '%s'\r\n", argv[1]);
        free(data); return 1;
    }
    /* now get all the info. Convert ELF section type into something meaningful */
    for(i = n = 0; i < ehdr->e_shnum; i++) {
        s = SECTION(i);
        s->sh_link = 0;
        if(!s->sh_size) continue;
        if(s->sh_type == SHT_PROGBITS && (s->sh_flags & SHF_ALLOC)) {
            s->sh_link = s->sh_flags & SHF_EXECINSTR ? SHT_CODE : (s->sh_flags & SHF_WRITE ? SHT_DATA : SHT_RODATA);
            if(s->sh_flags & SHF_EXECINSTR) n++;
        } else
        if(s->sh_type == SHT_NOBITS && s->sh_flags == (SHF_WRITE | SHF_ALLOC)) {
            s->sh_link = SHT_BSS;
        } else
        if(s->sh_type == SHT_NOTE) {
            if(!match) {
                ptr = (uint32_t*)(data + s->sh_offset);
                if(!ptr[0] && ptr[1] >= sizeof(plg_id_t) && (ptr[2] >> 16) == 0xEA57) {
                    plugin.type = ptr[2] & 0xff; match = (plg_id_t*)&ptr[3];
                    for(plugin.nid = 0; plugin.nid * sizeof(plg_id_t) < ptr[1] && match[plugin.nid].type; plugin.nid++);
                }
            }
        } else
        if(s->sh_type == SHT_REL || s->sh_type == SHT_RELA) {
            for(j = 0; j < s->sh_size / s->sh_entsize; j++) {
                rel = (Elf64_Rela *)(data + s->sh_offset + j * s->sh_entsize);
                for(k = 0; k < sizeof(rel_types)/sizeof(rel_types[0]); k++)
                    if(rel_types[k].arch == ehdr->e_machine && rel_types[k].type == ELF64_R_TYPE(rel->r_info)) {
                        /* count how many relocations we need to store in output */
                        if(rel_types[k].calc != REL_NONE && (rel_types[k].calc == REL_SA ||
                           !((Elf64_Sym *)(data + syms->sh_offset + ELF64_R_SYM(rel->r_info) * syms->sh_entsize))->st_shndx)) {
                            plugin.nrel++;
                            if(!plugin.nrel) {
                                fprintf(stderr, "plgld: too many relocations in '%s'\r\n", argv[1]);
                                free(data); return 1;
                            }
                        }
                        break;
                    }
                if(k >= sizeof(rel_types)/sizeof(rel_types[0])) {
                    sym = (Elf64_Sym *)(data + syms->sh_offset + ELF64_R_SYM(rel->r_info) * syms->sh_entsize);
                    fprintf(stderr, "plgld: relocation entry #%d offset %x sym '%s', unknown rela type %u for arch %u in '%s'\r\n", j,
                        (uint32_t)rel->r_offset, (char*)data + strs->sh_offset + sym->st_name,
                        (uint32_t)ELF64_R_TYPE(rel->r_info), ehdr->e_machine, argv[1]);
                    if(ehdr->e_machine == EM_X86_64 && ((uint32_t)ELF64_R_TYPE(rel->r_info) == R_X86_64_PLT32 ||
                      (uint32_t)ELF64_R_TYPE(rel->r_info) == R_X86_64_PLTOFF64 ||
                      (uint32_t)ELF64_R_TYPE(rel->r_info) == R_X86_64_GOTPLT64))
                        fprintf(stderr, "plgld: did you forget to use 'static' on a plugin local function?\r\n");
                    free(data); return 1;
                }
            }
        }
    }
    /* we shouldn't report missing code, instead we simply won't generate the plugin */
    if(!n) { /* fprintf(stderr, "plgld: no .text section '%s'\r\n", argv[1]); */ free(data); return 0; }
    if(!plugin.type || ((!plugin.nid || !match) && plugin.type != PLG_T_TAG)) { fprintf(stderr, "plgld: missing EASY_PLUGIN in source of '%s'\r\n", argv[1]); free(data); return 1; }
    if(plugin.nid > 60) { fprintf(stderr, "plgld: too many identify match records (max 60, must fit in one sector) in '%s'\r\n", argv[1]); free(data); return 1; }
    if(!plugin.nrel) { fprintf(stderr, "plgld: no relocation records in '%s'\r\n", argv[1]); free(data); return 1; }

    /* calculate true starting addresses */
    addr = code = hdrsz = sizeof(plg_hdr_t) + plugin.nid * sizeof(plg_id_t) + plugin.nrel * sizeof(plg_rel_t);
    for(l = -1U, j = SHT_CODE; j <= SHT_BSS; j++)
        for(i = 0; i < ehdr->e_shnum; i++) {
            s = SECTION(i);
            if(s->sh_link == j) {
                A = (addr + s->sh_addralign - 1) & ~(s->sh_addralign - 1); s->sh_addralign = 0;
                /* add alignment to the previous section's size. We don't really need it, only relevant for plg_dump() */
                if(addr != A) {
                    if(l != -1U) SECTION(l)->sh_addralign += A - addr;
                    else { code += A - addr; s->sh_addralign = A - addr; }
                    if(plugin.filesz < A) plugin.filesz = A;
                }
                addr = s->sh_addr = A; addr += s->sh_size; l = i;
                if(j != SHT_BSS) plugin.filesz = addr; else plugin.memsz = addr;
            }
        }
    if(plugin.memsz < plugin.filesz) plugin.memsz = plugin.filesz;
    plugin.memsz = (plugin.memsz + 7) & ~7;
    for(i = 0; i < ehdr->e_shnum; i++) {
        s = SECTION(i);
        if(s->sh_link == SHT_CODE) plugin.codesz += s->sh_size + s->sh_addralign;
        if(s->sh_link == SHT_RODATA) plugin.rosz += s->sh_size + s->sh_addralign;
    }
    /* get entry point */
    plugin.entry = ehdr->e_entry + code;
    for(j = 0; j < syms->sh_size; j += syms->sh_entsize) {
        sym = (Elf64_Sym *)(data + syms->sh_offset + j);
        if(!strcmp((char*)data + strs->sh_offset + sym->st_name, "_start")) { plugin.entry = sym->st_value + code; break; }
    }

    /* generate output */
    out = (uint8_t*)malloc(plugin.filesz);
    if(!out) { fprintf(stderr, "plgld: memory allocation error\r\n"); free(data); return 1; }
    memset(out, 0, plugin.filesz);
    memcpy(out, &plugin, sizeof(plg_hdr_t));
    if(plugin.nid) memcpy(out + sizeof(plg_hdr_t), match, plugin.nid * sizeof(plg_id_t));
    for(i = 0; i < ehdr->e_shnum; i++) {
        s = SECTION(i); if(s->sh_link && s->sh_link != SHT_BSS) { memcpy(out + s->sh_addr, data + s->sh_offset, s->sh_size); }
    }

    /* do relocations */
    rr = (plg_rel_t*)(out + sizeof(plg_hdr_t) + plugin.nid * sizeof(plg_id_t));
    for(i = 0; i < ehdr->e_shnum; i++) {
        s = SECTION(i);
        if(!s->sh_size || (s->sh_type != SHT_REL && s->sh_type != SHT_RELA)) continue;
        s2 = SECTION(s->sh_info);
        for(j = 0; j < s->sh_size / s->sh_entsize; j++) {
            rel = (Elf64_Rela *)(data + s->sh_offset + j * s->sh_entsize);
            sym = (Elf64_Sym *)(data + syms->sh_offset + ELF64_R_SYM(rel->r_info) * syms->sh_entsize);
            str = (char*)data + strs->sh_offset + sym->st_name;
            for(k = 0; k < sizeof(rel_types)/sizeof(rel_types[0]) &&
              (rel_types[k].arch != ehdr->e_machine || rel_types[k].type != ELF64_R_TYPE(rel->r_info)); k++);
            if(rel_types[k].calc == REL_NONE) continue;
            /* calculate address */
            S = sym->st_shndx ? SECTION(sym->st_shndx)->sh_addr + sym->st_value : 0;
            A = s->sh_type == SHT_RELA ? rel->r_addend : 0;
            P = rel->r_offset + (s2->sh_link == SHT_RODATA ? hdrsz + plugin.codesz : (s2->sh_link == SHT_DATA ? hdrsz + plugin.codesz + plugin.rosz : code));
            if(rel_types[k].calc & REL_G) addr = 0; else
            if(rel_types[k].calc & REL_Z) addr = sym->st_size; else
            if(rel_types[k].calc & REL_S) addr = S;
            if(rel_types[k].calc & REL_A) addr += A;
            if((rel_types[k].calc & REL_P) && !(rel_types[k].calc & REL_G)) addr -= P;
            /* external address? */
            symb = 0xff;
            if(!sym->st_shndx) {
                /* look up symbol */
                for(symb = 1; symb < sizeof(plg_got)/sizeof(plg_got[0]) && strcmp(plg_got[(int)symb], str); symb++);
                if(symb - 1 > plugin.ngot) plugin.ngot = symb - 1;
                if(symb >= PLG_NUMSYM) {
                    fprintf(stderr, "plgld: unresolved reference '%s' in '%s'\r\n", str, argv[1]);
                    free(data); free(out); return 1;
                }
                /* check if we can encode this relocation, must be either absolute or pc-relative */
                if(!(rel_types[k].calc & REL_G) && rel_types[k].calc != REL_SA && rel_types[k].calc != REL_SAP) {
                    fprintf(stderr, "plgld: relocation entry #%d offset %x symbol '%s', unsupported relocation %u for arch %u in '%s'\r\n",
                        j, (uint32_t)rel->r_offset, str, (uint32_t)ELF64_R_TYPE(rel->r_info), ehdr->e_machine, argv[1]);
                    free(data); free(out);
                    return 1;
                }
            } else
            /* internal address? */
            if(sym->st_shndx < ehdr->e_shnum) {
                /* add relocation record to output for absolute addresses only */
                symb = (rel_types[k].calc == REL_SA ? 0 : 0xff);
            } else {
                fprintf(stderr, "plgld: invalid section for symbol '%s' in '%s'\r\n", data + strs->sh_offset + sym->st_name, argv[1]);
                free(data); free(out); return 1;
            }
            /* do like this to preserve sign bits */
            addr /= (1L << rel_types[k].sbit);
            /* if addr is negative, negate it and set a bit. This effectively switches a MOVZ to a MOVN instruction */
            if(rel_types[k].neg) {
                *((uint64_t*)(out + P)) &= ~(1 << rel_types[k].neg);
                if(addr < 0) { *((uint64_t*)(out + P)) |= (1 << rel_types[k].neg); addr = ~addr; }
            }
            /* calculate immediate value and place in the instruction */
            for(m = 1, n = 0, l = rel_types[k].ebit - rel_types[k].sbit; m && n <= l; m <<= 1)
                /* immediate masks are architecture specific, but we simplify here because only ARM needs these */
                if(imasks[rel_types[k].imsk] & m) {
                    if((addr >> n) & 1) *((uint64_t*)(out + P)) |= m;
                    else *((uint64_t*)(out + P)) &= ~m;
                    n++;
                }
            /* add relocation record to output */
            if(symb != 0xff) {
                rr->offs = P;
                rr->type =
                    (rel_types[k].neg << 26) | (rel_types[k].ebit << 20) | (rel_types[k].sbit << 14) | (rel_types[k].imsk << 10) |
                    (rel_types[k].calc & REL_G ? PLG_A_GOTREL : 0) | (rel_types[k].calc & REL_P ? PLG_A_PCREL : PLG_A_ABS) | symb;
                rr++;
            }
        }
    }

    /* write out and free resources */
    writefileall(argv[2], out, plugin.filesz);
    free(data);
    free(out);
    return 0;
}
