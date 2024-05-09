/* Universal Disassembler Function Library
 * https://gitlab.com/bztsrc/udisasm
 *
 *        ----- (NOT YET) GENERATED FILE, DO NOT EDIT! -----
 *
 * Copyright (C) 2017 bzt (bztsrc@gitlab)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * @brief Disassembler source generated from x86_64.txt
 */

extern uint64_t dbg_label, buf_reloc;
extern uint8_t sys_fault;

enum {
    DA_BYTE,
    DA_WORD,
    DA_LONG,
    DA_QUAD,
    DA_SNGL,
    DA_DBLR,
    DA_EXTR,
    DA_SDEP,
    DA_NONE,
    DA_MODRM=0x80
};

enum {
    DA_INVALID,
    DA_E,
    DA_EInd,
    DA_Ew,
    DA_Eb,
    DA_R,
    DA_Rw,
    DA_Ri,
    DA_S,
    DA_Si,
    DA_A,
    DA_BX,
    DA_CL,
    DA_DX,
    DA_SI,
    DA_DI,
    DA_CR,
    DA_DR,
    DA_TR,
    DA_I,
    DA_Is,
    DA_Ib,
    DA_Ibs,
    DA_Iw,
    DA_Iq,
    DA_O,
    DA_Db,
    DA_Dl,
    DA_o1,
    DA_o3,
    DA_OS,
    DA_ST,
    DA_STI,
    DA_X,
    DA_XA,
    DA_Ril,
    DA_Iba,
    DA_ME,
    DA_Rq,
    DA2_A_Ri  = DA_A*256+DA_Ri,
    DA2_I_Ri  = DA_I*256+DA_Ri,
    DA2_Ib_Ri = DA_Ib*256+DA_Ri,
    DA2_Iq_Ri = DA_Iq*256+DA_Ri,
    DA2_Iq_Ib = DA_Iq*256+DA_Ib,
    DA2_E_R   = DA_E*256+DA_R,
    DA2_E_A   = DA_E*256+DA_A,
    DA2_O_A   = DA_O*256+DA_A,
    DA2_A_O   = DA_A*256+DA_O,
    DA2_Eb_R  = DA_Eb*256+DA_R,
    DA2_Ew_R  = DA_Ew*256+DA_R,
    DA2_Ei_R  = DA_EInd*256+DA_R,
    DA2_CL_E  = DA_CL*256+DA_E,
    DA2_R_E   = DA_R*256+DA_E,
    DA2_R_I   = DA_R*256+DA_I,
    DA2_I_E   = DA_I*256+DA_E,
    DA2_Ib_E  = DA_Ib*256+DA_E,
    DA2_Iw_E  = DA_Iw*256+DA_E,
    DA2_I_EInd= DA_I*256+DA_EInd,
    DA2_I_A   = DA_I*256+DA_A,
    DA2_Ib_A  = DA_Ib*256+DA_A,
    DA2_A_Ib  = DA_A*256+DA_Ib,
    DA2_DX_A  = DA_DX*256+DA_A,
    DA2_A_DX  = DA_A*256+DA_DX,
    DA2_CR_E  = DA_CR*256+DA_E,
    DA2_DR_E  = DA_DR*256+DA_E,
    DA2_TR_E  = DA_DR*256+DA_E,
    DA2_E_CR  = DA_E*256+DA_CR,
    DA2_E_DR  = DA_E*256+DA_DR,
    DA2_E_TR  = DA_E*256+DA_TR,
    DA2_S_Ew  = DA_S*256+DA_Ew,
    DA2_Ew_S  = DA_Ew*256+DA_S,
    DA2_STI_ST= DA_STI*256+DA_ST,
    DA2_ST_STI= DA_ST*256+DA_STI,
    DA2_SI_DI = DA_SI*256+DA_DI,
    DA2_DX_DI = DA_DX*256+DA_DI,
    DA2_SI_DX = DA_SI*256+DA_DX,
    DA2_o1_E  = DA_o1*256+DA_E,
    DA3_Ib_R_E= DA_Ib*65536+DA_R*256+DA_E,
    DA3_CL_R_E= DA_CL*65536+DA_R*256+DA_E,
    DA3_Ib_E_R= DA_Ib*65536+DA_E*256+DA_R,
    DA3_CL_E_R= DA_CL*65536+DA_E*256+DA_R,
    DA3_I_E_R = DA_I*65536+DA_E*256+DA_R
};

/* opcodes
 *  1 byte: inst_tbl
 *  2 bytes: first byte 0f, inst_tbl0f -> inst0fX
 */
typedef struct {
    char *name;
    uint8_t size;
    uint32_t mode;
    void *ext;
} instdesc;

char *grp1[] = {
    "add",
    "or",
    "adc",
    "sbb",
    "and",
    "sub",
    "xor",
    "cmp"
};

char *grp2[] = {
    "rol",
    "ror",
    "rcl",
    "rcr",
    "shl",
    "shr",
    "sal",
    "sar"
};

instdesc grp3[] = {
    { "test",   DA_NONE+DA_MODRM, DA2_I_E, 0 },
    { "test",   DA_NONE+DA_MODRM, DA2_I_E, 0 },
    { "not",    DA_NONE+DA_MODRM, DA_E, 0 },
    { "neg",    DA_NONE+DA_MODRM, DA_E, 0 },
    { "mul",    DA_NONE+DA_MODRM, DA2_E_A, 0 },
    { "imul",   DA_NONE+DA_MODRM, DA2_E_A, 0 },
    { "div",    DA_NONE+DA_MODRM, DA2_E_A, 0 },
    { "idiv",   DA_NONE+DA_MODRM, DA2_E_A, 0 },
};

instdesc grp4[] = {
    { "inc",    DA_BYTE+DA_MODRM, DA_E, 0 },
    { "dec",    DA_BYTE+DA_MODRM, DA_E, 0 },
    { "",       DA_NONE+DA_MODRM, 0, 0 },
    { "",       DA_NONE+DA_MODRM, 0, 0 },
    { "",       DA_NONE+DA_MODRM, 0, 0 },
    { "",       DA_NONE+DA_MODRM, 0, 0 },
    { "",       DA_NONE+DA_MODRM, 0, 0 },
    { "",       DA_NONE+DA_MODRM, 0, 0 }
};

instdesc grp5[] = {
    { "inc",    DA_LONG+DA_MODRM, DA_E, 0 },
    { "dec",    DA_LONG+DA_MODRM, DA_E, 0 },
    { "call",   DA_QUAD+DA_MODRM, DA_EInd, 0 },
    { "lcall",  DA_NONE+DA_MODRM, DA_EInd, 0 },
    { "jmp",    DA_NONE+DA_MODRM, DA_EInd, 0 },
    { "ljmp",   DA_NONE+DA_MODRM, DA_EInd, 0 },
    { "push",   DA_QUAD+DA_MODRM, DA_E, 0 },
    { "",       DA_NONE+DA_MODRM, 0, 0 }
};

char *grp6[] = {
    "sldt",
    "str",
    "lldt",
    "ltr",
    "verr",
    "verw",
    "",
    ""
};

char *inst0f00[] = {
    "invplg",
    "swapgs",
    "rdtscp",
    "",
    "",
    "",
    "",
    ""
};

instdesc grp7[] = {
    { "sgdt",   DA_QUAD+DA_MODRM, DA_ME, 0 },
    { "sidt",   DA_QUAD+DA_MODRM, DA_ME, 0 },
    { "lgdt",   DA_QUAD+DA_MODRM, DA_ME, 0 },
    { "lidt",   DA_QUAD+DA_MODRM, DA_ME, 0 },
    { "smsw",   DA_NONE+DA_MODRM, DA_Rw, 0 },
    { "",       DA_NONE+DA_MODRM, DA_Rw, 0 },
    { "lmsw",   DA_NONE+DA_MODRM, DA_Rw, 0 },
    { NULL,     DA_NONE+DA_MODRM, DA_Rw, inst0f00 }
};

char *grp8[] = {
    "",
    "",
    "",
    "",
    "bt",
    "bts",
    "btr",
    "btc"
};

char *grp9[] = {
    "",
    "cmpxchg8b",
    "",
    "",
    "",
    "",
    "",
    ""
};

char *grpA[] = {
    "",
    "cmpxchg8b",
    "",
    "",
    "",
    "",
    "",
    ""
};

char *grpB[] = {
    "xstorerng",
    "xcryptecb",
    "xcryptcbc",
    "",
    "xcryptcfb",
    "xcryptofb",
    "",
    ""
};

char *inst0f71[] = {
    "",
    "",
    "psrlw",
    "",
    "psraw",
    "",
    "psllw",
    ""
};

char *inst0f72[] = {
    "",
    "",
    "psrld",
    "",
    "psrad",
    "",
    "pslld",
    ""
};

char *inst0f73[] = {
    "",
    "psrlq",
    "psrldq",
    "psraq",
    "",
    "psllq",
    "pslldq"
};

char *inst0fae[] = {
    "sfence",
    "clflush",
    "",
    "",
    "",
    "",
    "",
    ""
};

instdesc inst0f0[] = {
    { NULL,     DA_NONE+DA_MODRM, DA_Ew, grp6 },
    { NULL,     DA_NONE+DA_MODRM, DA_Ew, grp7 },
    { "lar",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "lsl",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "",       DA_NONE, 0, 0 },
    { "syscall",DA_NONE, 0, 0 },
    { "clts",   DA_NONE, 0, 0 },
    { "sysret", DA_NONE, 0, 0 },

    { "invd",   DA_NONE, 0, 0 },
    { "wbinvd", DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 }
};

instdesc inst0f2[] = {
    { "mov",    DA_LONG+DA_MODRM, DA2_CR_E, 0 },
    { "mov",    DA_LONG+DA_MODRM, DA2_DR_E, 0 },
    { "mov",    DA_LONG+DA_MODRM, DA2_E_CR, 0 },
    { "mov",    DA_LONG+DA_MODRM, DA2_E_DR, 0 },
    { "mov",    DA_LONG+DA_MODRM, DA2_TR_E, 0 },
    { "",       DA_NONE, 0, 0 },
    { "mov",    DA_LONG+DA_MODRM, DA2_E_TR, 0 },
    { "",       DA_NONE, 0, 0 },

    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 }
};

instdesc inst0f3[] = {
    { "wrmsr",  DA_NONE, 0, 0 },
    { "rdtsc",  DA_NONE, 0, 0 },
    { "rdmsr",  DA_NONE, 0, 0 },
    { "rdpmc",  DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },

    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 }
};

instdesc inst0f7[] = {
    { "",       DA_NONE, 0, 0 },
    { NULL,     DA_NONE, 0, inst0f71 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },

    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 }
};

instdesc inst0f8[] = {
    { "jo",     DA_NONE, DA_Dl, 0 },
    { "jno",    DA_NONE, DA_Dl, 0 },
    { "jb",     DA_NONE, DA_Dl, 0 },
    { "jnb",    DA_NONE, DA_Dl, 0 },
    { "jz",     DA_NONE, DA_Dl, 0 },
    { "jnz",    DA_NONE, DA_Dl, 0 },
    { "jbe",    DA_NONE, DA_Dl, 0 },
    { "jnbe",   DA_NONE, DA_Dl, 0 },

    { "js",     DA_NONE, DA_Dl, 0 },
    { "jns",    DA_NONE, DA_Dl, 0 },
    { "jp",     DA_NONE, DA_Dl, 0 },
    { "jnp",    DA_NONE, DA_Dl, 0 },
    { "jl",     DA_NONE, DA_Dl, 0 },
    { "jnl",    DA_NONE, DA_Dl, 0 },
    { "jle",    DA_NONE, DA_Dl, 0 },
    { "jnle",   DA_NONE, DA_Dl, 0 }
};

instdesc inst0f9[] = {
    { "seto",   DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setno",  DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setb",   DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setnb",  DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setz",   DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setnz",  DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setbe",  DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setnbe", DA_NONE+DA_MODRM, DA_Eb, 0 },

    { "sets",   DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setns",  DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setp",   DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setnp",  DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setl",   DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setnl",  DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setle",  DA_NONE+DA_MODRM, DA_Eb, 0 },
    { "setnle", DA_NONE+DA_MODRM, DA_Eb, 0 }
};

instdesc inst0fa[] = {
    { "push",   DA_QUAD, DA_Si, 0 },
    { "pop",    DA_QUAD, DA_Si, 0 },
    { "cpuid",  DA_NONE, 0, 0 },
    { "bt",     DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "shld",   DA_LONG+DA_MODRM, DA3_Ib_R_E, 0 },
    { "shld",   DA_LONG+DA_MODRM, DA3_CL_R_E, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE+DA_MODRM, 0, grpB },

    { "push",   DA_QUAD, DA_Si, 0 },
    { "pop",    DA_QUAD, DA_Si, 0 },
    { "",       DA_NONE, 0, 0 },
    { "bts",    DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "shrd",   DA_LONG+DA_MODRM, DA3_Ib_R_E, 0 },
    { "shrd",   DA_LONG+DA_MODRM, DA3_CL_R_E, 0 },
    { NULL,     DA_NONE+DA_MODRM, DA_E, grp9 },
    { "imul",   DA_LONG+DA_MODRM, DA2_E_R, 0 }
};

instdesc inst0fb[] = {
    { "cmpxchg",DA_BYTE+DA_MODRM, DA2_R_E, 0 },
    { "cmpxchg",DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "lss",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "btr",    DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "lfs",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "lgs",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "movzb",  DA_LONG+DA_MODRM, DA2_Eb_R, 0 },
    { "movzw",  DA_LONG+DA_MODRM, DA2_Ew_R, 0 },

    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { NULL,     DA_LONG+DA_MODRM, DA2_Ib_E, grp8 },
    { "btc",    DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "bsf",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "bsr",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "movsb",  DA_LONG+DA_MODRM, DA2_Eb_R, 0 },
    { "movsw",  DA_LONG+DA_MODRM, DA2_Ew_R, 0 }
};

instdesc inst0fc[] = {
    { "xadd",   DA_BYTE+DA_MODRM, DA2_R_E, 0 },
    { "xadd",   DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { NULL,       DA_NONE+DA_MODRM, DA_E, grp9 },

    { "bswap",  DA_LONG, DA_Ril, 0 },
    { "bswap",  DA_LONG, DA_Ril, 0 },
    { "bswap",  DA_LONG, DA_Ril, 0 },
    { "bswap",  DA_LONG, DA_Ril, 0 },
    { "bswap",  DA_LONG, DA_Ril, 0 },
    { "bswap",  DA_LONG, DA_Ril, 0 },
    { "bswap",  DA_LONG, DA_Ril, 0 },
    { "bswap",  DA_LONG, DA_Ril, 0 }
};

char *esc92[] = {
    "fnop",
    "",
    "",
    "",
    "",
    "",
    "",
    ""
};

char *esc94[] = {
    "fchs",
    "fabs",
    "",
    "",
    "ftst",
    "fxam",
    "",
    ""
};

char *esc95[] = {
    "fld1",
    "fldl2t",
    "fldl2e",
    "fldpi",
    "fldlg2",
    "fldln2",
    "fldz",
    ""
};

char *esc96[] = {
    "f2xm1",
    "fyl2x",
    "fptan",
    "fpatan",
    "fxtract",
    "fprem1",
    "fdecstp",
    "fincstp"
};

char *esc97[] = {
    "fprem",
    "fyl2xp1",
    "fsqrt",
    "fsincos",
    "frndint",
    "fscale",
    "fsin",
    "fcos"
};

char *esca5[] = {
    "",
    "fucompp",
    "",
    "",
    "",
    "",
    "",
    ""
};

char *escb4[] = {
    "fneni",
    "fndisi",
    "fnclex",
    "fninit",
    "fsetpm",
    "",
    "",
    ""
};

char *esce3[] = {
    "",
    "fcompp",
    "",
    "",
    "",
    "",
    "",
    ""
};

char *escf4[] = {
    "fnstsw",
    "",
    "",
    "",
    "",
    "",
    "",
    ""
};

instdesc esc8[] = {
    { "fadd",   DA_SNGL, DA2_STI_ST, 0 },
    { "fmul",   DA_SNGL, DA2_STI_ST, 0 },
    { "fcom",   DA_SNGL, DA2_STI_ST, 0 },
    { "fcomp",  DA_SNGL, DA2_STI_ST, 0 },
    { "fsub",   DA_SNGL, DA2_STI_ST, 0 },
    { "fsubr",  DA_SNGL, DA2_STI_ST, 0 },
    { "fdiv",   DA_SNGL, DA2_STI_ST, 0 },
    { "fdivr",  DA_SNGL, DA2_STI_ST, 0 }
};

instdesc esc9[] = {
    { "fld",    DA_SNGL, DA_STI, 0 },
    { "",       DA_NONE, DA_STI, "fxch" },
    { "fst",    DA_SNGL, DA_X, esc92 },
    { "fstp",   DA_SNGL, DA_X, 0 },
    { "fldenv", DA_NONE, DA_X, esc94 },
    { "fldcw",  DA_NONE, DA_X, esc95 },
    { "fnstenv",DA_NONE, DA_X, esc96 },
    { "fnstcw", DA_NONE, DA_X, esc97 }
};

instdesc esca[] = {
    { "fiadd",  DA_LONG, 0, 0 },
    { "fimul",  DA_LONG, 0, 0 },
    { "ficom",  DA_LONG, 0, 0 },
    { "ficomp", DA_LONG, 0, 0 },
    { "fisub",  DA_LONG, DA_X, 0 },
    { "fisubr", DA_LONG, 0, 0 },
    { "fidiv",  DA_LONG, 0, 0 },
    { "fidivr", DA_LONG, 0, 0 }

};

instdesc escb[] = {
    { "fild",   DA_LONG, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "fist",   DA_LONG, 0, 0 },
    { "fistp",  DA_LONG, 0, 0 },
    { "",       DA_WORD, DA_X, escb4 },
    { "fld",    DA_EXTR, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "fstp",   DA_EXTR, 0, 0 }
};

instdesc escc[] = {
    { "fadd",   DA_DBLR, DA2_STI_ST, 0 },
    { "fmul",   DA_DBLR, DA2_STI_ST, 0 },
    { "fcom",   DA_DBLR, DA2_STI_ST, 0 },
    { "fcomp",  DA_DBLR, DA2_STI_ST, 0 },
    { "fsub",   DA_DBLR, DA2_STI_ST, 0 },
    { "fsubr",  DA_DBLR, DA2_STI_ST, 0 },
    { "fdiv",   DA_DBLR, DA2_STI_ST, 0 },
    { "fdivr",  DA_DBLR, DA2_STI_ST, 0 }
};

instdesc escd[] = {
    { "fld",    DA_DBLR, DA_STI, "ffree" },
    { "",       DA_NONE, 0, 0 },
    { "fst",    DA_DBLR, DA_STI, 0 },
    { "fstp",   DA_DBLR, DA_STI, 0 },
    { "frstor", DA_NONE, DA_STI, "fucom" },
    { "",       DA_NONE, DA_STI, "fucomp" },
    { "fnsave", DA_NONE, 0, 0 },
    { "fnstsw", DA_NONE, 0, 0 }
};

instdesc esce[] = {
    { "fiadd",  DA_WORD, DA2_ST_STI, "faddp" },
    { "fimul",  DA_WORD, DA2_ST_STI, "fmulp" },
    { "ficom",  DA_WORD, 0, 0 },
    { "ficomp", DA_WORD, DA_X, esce3 },
    { "fisub",  DA_WORD, DA2_ST_STI, "fsubrp" },
    { "fisubr", DA_WORD, DA2_ST_STI, "fsubp" },
    { "fidiv",  DA_WORD, DA2_ST_STI, "fdivrp" },
    { "fidivr", DA_WORD, DA2_ST_STI, "fdivp" }
};

instdesc escf[] = {
    { "fild",   DA_WORD, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "fist",   DA_WORD, 0, 0 },
    { "fistp",  DA_WORD, 0, 0 },
    { "fbld",   DA_NONE, DA_XA, escf4 },
    { "fild",   DA_QUAD, 0, 0 },
    { "fbstp",  DA_NONE, 0, 0 },
    { "fistp",  DA_QUAD, 0, 0 }
};

instdesc inst_tbl[] = {
    { "add",    DA_BYTE+DA_MODRM, DA2_R_E, 0 },     /*00*/
    { "add",    DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "add",    DA_BYTE+DA_MODRM, DA2_E_R, 0 },
    { "add",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "add",    DA_BYTE, DA2_Ib_A, 0 },
    { "add",    DA_LONG, DA2_I_A, 0 },
    { "",       DA_NONE, DA_Si, 0 },
    { "",       DA_NONE, DA_Si, 0 },

    { "or",     DA_BYTE+DA_MODRM, DA2_R_E, 0 },     /*08*/
    { "or",     DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "or",     DA_BYTE+DA_MODRM, DA2_E_R, 0 },
    { "or",     DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "or",     DA_BYTE, DA2_Ib_A, 0 },
    { "or",     DA_LONG, DA2_I_A, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },

    { "adc",    DA_BYTE+DA_MODRM, DA2_R_E, 0 },     /*10*/
    { "adc",    DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "adc",    DA_BYTE+DA_MODRM, DA2_E_R, 0 },
    { "adc",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "adc",    DA_BYTE, DA2_Ib_A, 0 },
    { "adc",    DA_LONG, DA2_I_A, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },

    { "sbb",    DA_BYTE+DA_MODRM, DA2_R_E, 0 },     /*18*/
    { "sbb",    DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "sbb",    DA_BYTE+DA_MODRM, DA2_E_R, 0 },
    { "sbb",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "sbb",    DA_BYTE, DA2_Ib_A, 0 },
    { "sbb",    DA_LONG, DA2_I_A, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },

    { "and",    DA_BYTE+DA_MODRM, DA2_R_E, 0 },     /*20*/
    { "and",    DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "and",    DA_BYTE+DA_MODRM, DA2_E_R, 0 },
    { "and",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "and",    DA_BYTE, DA2_Ib_A, 0 },
    { "and",    DA_LONG, DA2_I_A, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },

    { "sub",    DA_BYTE+DA_MODRM, DA2_R_E, 0 },     /*28*/
    { "sub",    DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "sub",    DA_BYTE+DA_MODRM, DA2_E_R, 0 },
    { "sub",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "sub",    DA_BYTE, DA2_Ib_A, 0 },
    { "sub",    DA_LONG, DA2_I_A, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },

    { "xor",    DA_BYTE+DA_MODRM, DA2_R_E, 0 },     /*30*/
    { "xor",    DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "xor",    DA_BYTE+DA_MODRM, DA2_E_R, 0 },
    { "xor",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "xor",    DA_BYTE, DA2_Ib_A, 0 },
    { "xor",    DA_LONG, DA2_I_A, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },

    { "cmp",    DA_BYTE+DA_MODRM, DA2_R_E, 0 },     /*38*/
    { "cmp",    DA_BYTE+DA_MODRM, DA2_R_E, 0 },
    { "cmp",    DA_BYTE+DA_MODRM, DA2_E_R, 0 },
    { "cmp",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "cmp",    DA_BYTE, DA2_Ib_A, 0 },
    { "cmp",    DA_LONG, DA2_I_A, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },

    { "",       DA_LONG, DA_Ri, 0 },                /*40*/
    { "",       DA_LONG, DA_Ri, 0 },
    { "",       DA_LONG, DA_Ri, 0 },
    { "",       DA_LONG, DA_Ri, 0 },
    { "",       DA_LONG, DA_Ri, 0 },
    { "",       DA_LONG, DA_Ri, 0 },
    { "",       DA_LONG, DA_Ri, 0 },
    { "",       DA_LONG, DA_Ri, 0 },

    { "",       DA_LONG, DA_Ri, 0 },                /*48*/
    { "",       DA_LONG, DA_Ri, 0 },
    { "",       DA_LONG, DA_Ri, 0 },
    { "",       DA_LONG, DA_Ri, 0 },
    { "",       DA_LONG, DA_Ri, 0 },
    { "",       DA_LONG, DA_Ri, 0 },
    { "",       DA_LONG, DA_Ri, 0 },
    { "",       DA_LONG, DA_Ri, 0 },

    { "push",   DA_QUAD, DA_Rq, 0 },                /*50*/
    { "push",   DA_QUAD, DA_Rq, 0 },
    { "push",   DA_QUAD, DA_Rq, 0 },
    { "push",   DA_QUAD, DA_Rq, 0 },
    { "push",   DA_QUAD, DA_Rq, 0 },
    { "push",   DA_QUAD, DA_Rq, 0 },
    { "push",   DA_QUAD, DA_Rq, 0 },
    { "push",   DA_QUAD, DA_Rq, 0 },

    { "pop",    DA_QUAD, DA_Rq, 0 },                /*58*/
    { "pop",    DA_QUAD, DA_Rq, 0 },
    { "pop",    DA_QUAD, DA_Rq, 0 },
    { "pop",    DA_QUAD, DA_Rq, 0 },
    { "pop",    DA_QUAD, DA_Rq, 0 },
    { "pop",    DA_QUAD, DA_Rq, 0 },
    { "pop",    DA_QUAD, DA_Rq, 0 },
    { "pop",    DA_QUAD, DA_Rq, 0 },

    { "",       DA_LONG, 0, 0 },                    /*60*/
    { "",       DA_LONG, 0, 0 },
    { "",       DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "movsxd", DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },

    { "push",   DA_LONG, DA_I, 0 },                 /*68*/
    { "imul",   DA_LONG+DA_MODRM, DA3_I_E_R, 0 },
    { "push",   DA_QUAD, DA_Ib, 0 },
    { "imul",   DA_LONG+DA_MODRM, DA3_Ib_E_R, 0 },
    { "ins",    DA_BYTE, DA2_DX_DI, 0 },
    { "ins",    DA_LONG, DA2_DX_DI, 0 },
    { "outs",   DA_BYTE, DA2_SI_DX, 0 },
    { "outs",   DA_LONG, DA2_SI_DX, 0 },

    { "jo",     DA_NONE, DA_Db, 0 },                /*70*/
    { "jno",    DA_NONE, DA_Db, 0 },
    { "jb",     DA_NONE, DA_Db, 0 },
    { "jnb",    DA_NONE, DA_Db, 0 },
    { "jz",     DA_NONE, DA_Db, 0 },
    { "jnz",    DA_NONE, DA_Db, 0 },
    { "jbe",    DA_NONE, DA_Db, 0 },
    { "jnbe",   DA_NONE, DA_Db, 0 },

    { "js",     DA_NONE, DA_Db, 0 },                /*78*/
    { "jns",    DA_NONE, DA_Db, 0 },
    { "jp",     DA_NONE, DA_Db, 0 },
    { "jnp",    DA_NONE, DA_Db, 0 },
    { "jl",     DA_NONE, DA_Db, 0 },
    { "jnl",    DA_NONE, DA_Db, 0 },
    { "jle",    DA_NONE, DA_Db, 0 },
    { "jnle",   DA_NONE, DA_Db, 0 },

    { NULL,     DA_BYTE+DA_MODRM, DA2_Ib_E, grp1 },  /*80*/
    { NULL,     DA_LONG+DA_MODRM, DA2_I_E, grp1 },
    { NULL,     DA_BYTE+DA_MODRM, DA2_Ib_E, grp1 },
    { NULL,     DA_LONG+DA_MODRM, DA2_Ib_E, grp1 },
    { "test",   DA_BYTE+DA_MODRM, DA2_R_E, 0 },
    { "test",   DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "xchg",   DA_BYTE+DA_MODRM, DA2_R_E, 0 },
    { "xchg",   DA_LONG+DA_MODRM, DA2_R_E, 0 },

    { "mov",    DA_BYTE+DA_MODRM, DA2_R_E, 0 },     /*88*/
    { "mov",    DA_LONG+DA_MODRM, DA2_R_E, 0 },
    { "mov",    DA_BYTE+DA_MODRM, DA2_E_R, 0 },
    { "mov",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "mov",    DA_NONE+DA_MODRM, DA2_S_Ew, 0 },
    { "lea",    DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "mov",    DA_NONE+DA_MODRM, DA2_Ew_S, 0 },
    { "pop",    DA_QUAD+DA_MODRM, DA_E, 0 },

    { "nop",    DA_NONE, 0, 0 },                    /*90*/
    { "xchg",   DA_LONG, DA2_A_Ri, 0 },
    { "xchg",   DA_LONG, DA2_A_Ri, 0 },
    { "xchg",   DA_LONG, DA2_A_Ri, 0 },
    { "xchg",   DA_LONG, DA2_A_Ri, 0 },
    { "xchg",   DA_LONG, DA2_A_Ri, 0 },
    { "xchg",   DA_LONG, DA2_A_Ri, 0 },
    { "xchg",   DA_LONG, DA2_A_Ri, 0 },

    { "cbw",    DA_SDEP, 0, "cdqe" },               /*98*/
    { "cwd",    DA_SDEP, 0, "cqo" },
    { "",       DA_NONE, DA_OS, 0 },
    { "wait",   DA_NONE, 0, 0 },
    { "pushf",  DA_QUAD, 0, 0 },
    { "popf",   DA_QUAD, 0, 0 },
    { "sahf",   DA_NONE, 0, 0 },
    { "lahf",   DA_NONE, 0, 0 },

    { "mov",    DA_BYTE, DA2_O_A, 0 },              /*A0*/
    { "mov",    DA_LONG, DA2_O_A, 0 },
    { "mov",    DA_BYTE, DA2_A_O, 0 },
    { "mov",    DA_LONG, DA2_A_O, 0 },
    { "movsb",  DA_BYTE, 0, 0 },
    { "movsd",  DA_LONG, 0, 0 },
    { "cmpsb",  DA_BYTE, 0, 0 },
    { "cmpsd",  DA_LONG, 0, 0 },

    { "test",   DA_BYTE, DA2_I_A, 0 },              /*A8*/
    { "test",   DA_LONG, DA2_I_A, 0 },
    { "stosb",  DA_BYTE, 0, 0 },
    { "stosw",  DA_WORD, 0, 0 },
    { "lodsb",  DA_BYTE, 0, 0 },
    { "lodsw",  DA_WORD, 0, 0 },
    { "scasb",  DA_BYTE, 0, 0 },
    { "scasd",  DA_LONG, 0, 0 },

    { "mov",    DA_BYTE, DA2_Ib_Ri, 0 },            /*B0*/
    { "mov",    DA_BYTE, DA2_Ib_Ri, 0 },
    { "mov",    DA_BYTE, DA2_Ib_Ri, 0 },
    { "mov",    DA_BYTE, DA2_Ib_Ri, 0 },
    { "mov",    DA_BYTE, DA2_Ib_Ri, 0 },
    { "mov",    DA_BYTE, DA2_Ib_Ri, 0 },
    { "mov",    DA_BYTE, DA2_Ib_Ri, 0 },
    { "mov",    DA_BYTE, DA2_Ib_Ri, 0 },

    { "mov",    DA_LONG, DA2_I_Ri, 0 },             /*B8*/
    { "mov",    DA_LONG, DA2_I_Ri, 0 },
    { "mov",    DA_LONG, DA2_I_Ri, 0 },
    { "mov",    DA_LONG, DA2_I_Ri, 0 },
    { "mov",    DA_LONG, DA2_Iq_Ri, 0 },
    { "mov",    DA_LONG, DA2_Iq_Ri, 0 },
    { "mov",    DA_LONG, DA2_Iq_Ri, 0 },
    { "mov",    DA_LONG, DA2_Iq_Ri, 0 },

    { NULL,     DA_BYTE+DA_MODRM, DA2_Ib_E, grp2 }, /*C0*/
    { NULL,     DA_LONG+DA_MODRM, DA2_Ib_E, grp2 },
    { "ret",    DA_NONE, DA_Iw, 0 },
    { "ret",    DA_NONE, 0, 0 },
    { "",       DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "",       DA_LONG+DA_MODRM, DA2_E_R, 0 },
    { "mov",    DA_BYTE+DA_MODRM, DA2_I_E, 0 },
    { "mov",    DA_LONG+DA_MODRM, DA2_I_E, 0 },

    { "enter",  DA_NONE, DA2_Iq_Ib, 0 },            /*C8*/
    { "leave",  DA_NONE, 0, 0 },
    { "lret",   DA_NONE, DA_Iw, 0 },
    { "lret",   DA_NONE, 0, 0 },
    { "int",    DA_NONE, DA_o3, 0 },
    { "int",    DA_NONE, DA_Ib, 0 },
    { "",       DA_NONE, 0, 0 },
    { "iret",   DA_NONE, 0, 0 },

    { NULL,     DA_BYTE+DA_MODRM, DA2_o1_E, grp2 }, /*D0*/
    { NULL,     DA_LONG+DA_MODRM, DA2_o1_E, grp2 },
    { NULL,     DA_BYTE+DA_MODRM, DA2_CL_E, grp2 },
    { NULL,     DA_LONG+DA_MODRM, DA2_CL_E, grp2 },
    { "",       DA_NONE+DA_MODRM, DA_Ib, 0 },
    { "",       DA_NONE+DA_MODRM, DA_Ib, 0 },
    { "",       DA_NONE, 0, 0 },
    { "xlat",   DA_BYTE, DA_BX, 0 },

    { "",       DA_NONE+DA_MODRM, 0, esc8 },        /*D8*/
    { "",       DA_NONE+DA_MODRM, 0, esc9 },
    { "",       DA_NONE+DA_MODRM, 0, esca },
    { "",       DA_NONE+DA_MODRM, 0, escb },
    { "",       DA_NONE+DA_MODRM, 0, escc },
    { "",       DA_NONE+DA_MODRM, 0, escd },
    { "",       DA_NONE+DA_MODRM, 0, esce },
    { "",       DA_NONE+DA_MODRM, 0, escf },

    { "loopne", DA_NONE, DA_Db, 0 },                /*E0*/
    { "loope",  DA_NONE, DA_Db, 0 },
    { "loop",   DA_NONE, DA_Db, 0 },
    { "jcxz",   DA_SDEP, DA_Db, "jrcxz" },
    { "in",     DA_BYTE, DA2_Ib_A, 0 },
    { "in",     DA_LONG, DA2_Ib_A, 0 },
    { "out",    DA_BYTE, DA2_A_Ib, 0 },
    { "out",    DA_LONG, DA2_A_Ib, 0 },

    { "call",   DA_LONG, DA_Dl, 0 },                /*E8*/
    { "jmp",    DA_LONG, DA_Dl, 0 },
    { "",       DA_NONE, DA_OS, 0 },
    { "jmp",    DA_NONE, DA_Db, 0 },
    { "in",     DA_BYTE, DA2_DX_A, 0 },
    { "in",     DA_LONG, DA2_DX_A, 0 },
    { "out",    DA_BYTE, DA2_A_DX, 0 },
    { "out",    DA_LONG, DA2_A_DX, 0 },

    { "",       DA_NONE, 0, 0 },                    /*F0*/
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "",       DA_NONE, 0, 0 },
    { "hlt",    DA_NONE, 0, 0 },
    { "cmc",    DA_NONE, 0, 0 },
    { NULL,     DA_BYTE+DA_MODRM, 0, grp3 },
    { NULL,     DA_LONG+DA_MODRM, 0, grp3 },

    { "clc",    DA_NONE, 0, 0 },                    /*F8*/
    { "stc",    DA_NONE, 0, 0 },
    { "cli",    DA_NONE, 0, 0 },
    { "sti",    DA_NONE, 0, 0 },
    { "cld",    DA_NONE, 0, 0 },
    { "std",    DA_NONE, 0, 0 },
    { NULL,     DA_NONE+DA_MODRM, 0, grp4 },
    { NULL,     DA_NONE+DA_MODRM, 0, grp5 }
};

instdesc *inst_tbl0f[] = {
    inst0f0,
    0,
    inst0f2,
    inst0f3,
    0,
    0,
    0,
    inst0f7,
    inst0f8,
    inst0f9,
    inst0fa,
    inst0fb,
    inst0fc,
    0,
    0,
    0
};

char *sizestr[7] = {
    "byte",
    "word",
    "dword",
    "qword",
    "single",
    "double",
    "extend"
};

char *regs[4][16] = {
    { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh", "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"},
    { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di", "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"},
    { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi", "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"},
    { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15" }
};

char *sregs[8] = {
    "es", "cs", "ss", "ds", "fs", "gs", "?", "?"
};
char *unkmnemonic = "???";

/* cím feloldás */
uint8_t addr_reg;
uint8_t addr_scale;
uint64_t addr_disp;
uint8_t addr_seg;
uint8_t addr_saddr;
char *addr_base;
char *addr_idx;

static char *disasm_x86_prtaddr(char *str, uint64_t addr, uint8_t mode, uint8_t size, uint8_t reg)
{
    if(addr_reg) {
        if(size > 3) size = 3;
        str += sprintf(str, regs[size][reg]);
    } else {
        if(size > 6) size = 6;
        str += sprintf(str,"%s ", sizestr[size]);
        if (addr_seg)
            str += sprintf(str, "%cs:", addr_seg);
        str += sprintf(str,"[");
        if(addr_base!=NULL) {
            str += sprintf(str,"%s", addr_base);
            if(addr_idx!=NULL) {
                str += sprintf(str,"+%s", addr_idx);
                if(addr_scale>1)
                    str += sprintf(str,"*%d", addr_scale);
            }
        } else {
            if(mode!=DA_E)
                addr_disp += addr;
        }
        if(addr_disp) {
            str += sprintf(str,"%s%x", addr_base!=NULL?"+":"", addr_disp - (mode == DA_EInd ? buf_reloc : 0));
            if(addr_base==NULL)
                dbg_label = addr_disp;
        } else if(addr_base==NULL) *str++ = '0';
        str += sprintf(str,"]");
    }
    return str;
}

static uint64_t disasm_x86(uint64_t addr, char *str)
{
    uint64_t disasmstart;
    uint8_t rex = 0;
    uint8_t opcode = 0, opcode2 = 0;
    uint8_t modrm = 0;
    uint8_t sib = 0;
    uint64_t imm = 0;
    uint64_t disp = 0;
    uint8_t size = DA_LONG;
    uint8_t reg = 0;
    instdesc *inst;
    uint32_t i_size = DA_NONE;
    uint32_t i_mode;
    uint8_t mod, rm;
    uint16_t idx;
    char *i_name;

    disasmstart = addr;
    addr_seg = 0;
    addr_saddr = DA_QUAD;
    addr_base = NULL;
    addr_idx = NULL;
    dbg_label=0;

    sys_fault = false;
getprefix:
    opcode = *((uint8_t*)addr);
    if(sys_fault)
        goto end;
    addr++;
    /* szegmens felülbírálás CS, DS, ES, SS nem használt long módban */
    if(opcode==0x26||opcode==0x36||opcode==0x2e||opcode==0x3e)
        goto getprefix;
    /* fs */
    if(opcode==0x64) {
        addr_seg='f';
        goto getprefix;
    }
    /* gs */
    if(opcode==0x65) {
        addr_seg='g';
        goto getprefix;
    }
    /* adatméret módosító prefix */
    if(opcode==0x66) {
        rex=0x40;
        size=DA_WORD;
        goto getprefix;
    }
    /* címméret módosító */
    if(opcode==0x67) {
        addr_saddr=DA_LONG;
        goto getprefix;
    }
    if(opcode==0xf0) {
        if(str!=NULL)
            str += sprintf(str, "lock ");
        goto getprefix;
    }
    if(opcode==0xf2) {
        if(str!=NULL)
            str += sprintf(str, "repnz ");
        goto getprefix;
    }
    if(opcode==0xf3) {
        if(str!=NULL)
            str += sprintf(str, "repe ");
        goto getprefix;
    }
    /* rex */
    if((opcode&0xF0)==0x40) {
        rex = opcode;
        if(rex&0x08)
            size=DA_QUAD;
        goto getopcode;
    }
    goto prefixend;

getopcode:
    opcode = *((uint8_t*)addr);
    if(sys_fault)
        goto end;
    addr++;
prefixend:
    /* nop */
    if(opcode==0x90) {
        i_size = 1;
        while(*((uint8_t*)addr)==0x90) { i_size++; addr++; }
        if(str != NULL)
            str += sprintf(str, " %d x nop", i_size);
        goto end2;
    }
    /* 2 bájtos opkód? */
    if (opcode == 0x0f) {
        opcode2 = *((uint8_t*)addr);
        if(sys_fault)
            goto end;
        addr++;
        inst = inst_tbl0f[opcode2>>4];
        if (inst != NULL)
            inst = &inst[opcode2&0xf];
    } else {
        inst = &inst_tbl[opcode];
    }
    if (inst == NULL) {
unknown:
        if(str != NULL)
            str += sprintf(str, unkmnemonic);
        goto end;
    }
    /* van modrm? */
    if(inst->size & DA_MODRM) {
        modrm = *((uint8_t*)addr);
        addr++;
        mod = (modrm>>6) & 3;
        rm = ((rex & 0x01)<<3) + (modrm & 7);
        reg = ((rex & 0x04)<<1) + ((modrm>>3) & 7);
        addr_disp = 0;
        addr_base = addr_idx = NULL;
        addr_scale = addr_reg = false;
        if(mod==3) {
            addr_reg = true;
            addr_disp = (uint64_t)((uint8_t)rm);
        } else {
            /* van sib bájt? */
            if((modrm & 7)==0x04) {
                sib = *((uint8_t*)addr);
                addr++;
                addr_scale=sib>>6;
                idx=((rex&2)<<2)+((sib>>3)&7);
                if(idx!=4)
                    addr_idx=regs[DA_QUAD][idx];
                rm = ((rex&1)<<3)+(sib&7);
            }
            addr_base = regs[DA_QUAD][rm];
            if(mod==0) {
                if(rm==5) {
                    addr_disp = (uint64_t)(*((uint32_t*)addr));
                    addr += 4;
                    addr_base = NULL;
                }
            } else
            if(mod==1) {
                addr_disp = (uint64_t)(*((uint8_t*)addr));
                addr++;
            } else
            if(mod==2) {
                addr_disp = (uint64_t)(*((uint32_t*)addr));
                addr += 4;
            }
        }
    }
    /* ModRM specifikus */
    if((inst->name==NULL||inst->name[0]==0) && inst->ext != 0) {
        if(opcode>=0xd0&&opcode<=0xd8){
            /* esc */
            inst = (instdesc*)inst->ext;
            inst = &inst[(modrm>>3)&7];
            goto unknown;
        } else {
            /* typeof inst->ext == instdesc*? */
            if(inst->ext==grp3 || inst->ext==grp4 || inst->ext==grp5 ||
               inst->ext==grp7) {
                inst = (instdesc*)inst->ext;
                inst = &inst[(modrm>>3)&7];
            }
        }
    }
    if(inst==NULL) goto unknown;
    i_name = inst->name!=NULL ? inst->name : ((char**)inst->ext)[reg];
    i_size = inst->size&0xf;
    i_mode = inst->mode;
    if(i_name==NULL||i_name[0]==0) goto unknown;

    /* mnemonic */
    if (i_size == DA_SDEP) {
        if (size == DA_LONG)
            i_name=(char *)inst->ext;
        i_size = size;
    }
    if(rex)
        i_size = size;
    if(str!=NULL)
        str += sprintf(str, "%-6s\t", i_name);

    /* argumentumok */
    for (;i_mode != 0; i_mode >>= 8) {
        if (str!=NULL && *(str-1)!='\t')
            str += sprintf(str, ", ");

        switch (i_mode & 0xFF) {
            case DA_E:
            case DA_EInd:
                if(str != NULL)
                    str = disasm_x86_prtaddr(str, addr, i_mode&0xFF, i_size, addr_disp);
                break;
            case DA_Ew:
                if(str != NULL)
                    str = disasm_x86_prtaddr(str, addr, i_mode&0xFF, DA_WORD, reg);
                break;
            case DA_Eb:
                if(str != NULL)
                    str = disasm_x86_prtaddr(str, addr, i_mode&0xFF, DA_BYTE, reg);
                break;
            case DA_ME:
                if(str != NULL)
                    str += sprintf(str, addr_base ? addr_base : "0");
                break;
            case DA_R:
                if(str != NULL)
                    str += sprintf(str, regs[i_size][reg]);
                break;
            case DA_Rw:
                if(str != NULL)
                    str += sprintf(str, regs[DA_WORD][reg]);
                break;
            case DA_Ri:
                if(str != NULL)
                    str += sprintf(str, regs[i_size][(opcode&7)+((rex&1)<<3)]);
                break;
            case DA_Rq:
                if(str != NULL)
                    str += sprintf(str, regs[DA_QUAD][(opcode&7)+((rex&1)<<3)]);
                break;
            case DA_Ril:
                if(str != NULL)
                    str += sprintf(str, regs[DA_LONG][(opcode&7)+((rex&1)<<3)]);
                break;
            case DA_S:
                if(str != NULL)
                    str += sprintf(str, sregs[(modrm&7)]);
                break;
            case DA_Si:
                if(str != NULL)
                    str += sprintf(str, sregs[(opcode&7)]);
                break;
            case DA_A:
                if(str != NULL)
                    str += sprintf(str, i_size < 4 ? regs[i_size][0] : "???");
                break;
            case DA_BX:
                if(str != NULL) {
                    if (addr_seg)
                        str += sprintf(str, "%cs:", addr_seg);
                    str += sprintf(str,"%s [%s]", i_size < 4 ? sizestr[i_size] : "???", regs[addr_saddr][3]);
                }
                break;
            case DA_CL:
                if(str != NULL)
                    str += sprintf(str, regs[DA_BYTE][1]);
                break;
            case DA_DX:
                if(str != NULL)
                    str += sprintf(str, regs[DA_WORD][2]);
                break;
            case DA_SI:
                if(str != NULL) {
                    if (addr_seg)
                        str += sprintf(str, "%cs:", addr_seg);
                    str += sprintf(str,"%s [%s]", sizestr[i_size], regs[addr_saddr][6]);
                }
                break;
            case DA_DI:
                if(str != NULL) {
                    if (addr_seg)
                        str += sprintf(str, "%cs:", addr_seg);
                    str += sprintf(str,"%s [%s]", sizestr[i_size], regs[addr_saddr][7]);
                }
                break;
            case DA_CR:
                if (str != NULL)
                    str += sprintf(str, "cr%d", reg);
                break;
            case DA_DR:
                if (str != NULL)
                    str += sprintf(str, "dr%d", reg);
                break;
            case DA_TR:
                if (str != NULL)
                    str += sprintf(str, "tr%d", reg);
                break;
            case DA_I:
            case DA_Is:
                if(i_size == DA_WORD) {
                    imm = (uint64_t)(*((int16_t*)addr));
                    addr += 2;
                } else {
                    imm = (uint64_t)(*((int32_t*)addr));
                    addr += 4;
                }
                if (str != NULL)
                    str += sprintf(str, "%xh", imm);
                if(!dbg_label)
                    dbg_label=imm;
                break;
            case DA_Ib:
                imm = (uint64_t)(*((uint8_t*)addr));
                addr++;
                if (str != NULL)
                    str += sprintf(str, "%1xh", imm);
                if(!dbg_label)
                    dbg_label=imm;
                break;
            case DA_Iba:
            case DA_Ibs:
                imm = (uint64_t)(*((uint8_t*)addr));
                addr++;
                if (str != NULL && imm !=0x0a)
                    str += sprintf(str, "%1xh", imm);
                if(!dbg_label)
                    dbg_label=imm;
                break;
            case DA_Iw:
                imm = (uint64_t)(*((uint16_t*)addr));
                addr += 2;
                if (str != NULL)
                    str += sprintf(str, "%2xh", imm);
                if(!dbg_label)
                    dbg_label=imm;
                break;
            case DA_Iq:
                imm = (uint64_t)(*((uint64_t*)addr));
                addr += 8;
                if (str != NULL)
                    str += sprintf(str, "%8xh", imm);
                if(!dbg_label)
                    dbg_label=imm;
                break;
            case DA_O:
/*
                if (addr_saddr==DA_QUAD) {
                    disp = (uint64_t)(*((uint32_t*)addr));
                    addr += 4;
                } else {
*/
                    disp = (uint64_t)(*((uint16_t*)addr));
                    addr += 2;
/*                } */
                if(str != NULL) {
                    if (addr_seg)
                        str += sprintf(str, "%cs:", addr_seg);
                    str += sprintf(str,"%xh", disp);
                }
                if(!dbg_label)
                    dbg_label = disp;
                break;
            case DA_Db:
                disp = (uint64_t)(*((int8_t*)addr));
                addr++;
                disp += addr;
                if(str != NULL)
                    str += sprintf(str, "%xh", disp);
                if(!dbg_label)
                    dbg_label=disp;
                break;
            case DA_Dl:
/*
                if (i_size==DA_QUAD) {
                    disp = (uint64_t)(*((uint64_t*)addr));
                    addr+=8;
                } else if (i_size==DA_LONG) {
*/
                    disp = (uint64_t)(*((int32_t*)addr));
                    addr+=4;
/*
                } else if (i_size==DA_WORD) {
                    disp = (uint64_t)(*((int16_t*)addr));
                    addr+=2;
                } else if (i_size==DA_BYTE) {
                    disp = (uint64_t)(*((int8_t*)addr));
                    addr++;
                }
*/
                disp += addr;
                if(str != NULL)
                    str += sprintf(str, "%xh", disp);
                if(!dbg_label)
                    dbg_label=disp;
                break;
            case DA_o1:
                if(str != NULL)
                    str += sprintf(str, "1");
                break;
            case DA_o3:
                if(str != NULL)
                    str += sprintf(str, "3");
                break;
            case DA_OS:
                if (size==DA_QUAD) {
                    disp = (uint64_t)(*((int64_t*)addr));
                    addr+=8;
                } else if (size==DA_LONG) {
                    disp = (uint64_t)(*((int32_t*)addr));
                    addr+=4;
                } else if (size==DA_WORD) {
                    disp = (uint64_t)(*((int16_t*)addr));
                    addr+=2;
                } else if (size==DA_BYTE) {
                    disp = (uint64_t)(*((int8_t*)addr));
                    addr++;
                }
                imm = (uint64_t)(*((uint16_t*)addr));
                addr+=2;
                if(str != NULL)
                    str += sprintf(str, "%x:%xh", imm, disp);
                break;
            default:
                if(str != NULL)
                    str += sprintf(str, "?");
                break;
        }
    }

end:
    if (addr - disasmstart > 15 && str!=NULL)
        str += sprintf(str," <too long>");
end2:
    if(str!=NULL)
        *str=0;
    return addr;
}
