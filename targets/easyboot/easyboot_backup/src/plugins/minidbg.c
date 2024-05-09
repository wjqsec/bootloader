/*
 * src/plugins/minidbg.c
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
 * @brief Implements a Mini Debugger
 * https://gitlab.com/bztsrc/minidbg
 */

#include "../loader.h"

EASY_PLUGIN(PLG_T_TAG) {};

uint8_t sys_fault=0;
uint64_t dbg_label=0, buf_reloc=0, *dbg_regs = (uint64_t*)0x580;
char dbg_cmd[256], dbg_running=0;
static void dbg_printf(char *fmt, ...);
static unsigned int sprintf(char *dst, char* fmt, ...);
#ifdef __aarch64__
#include "../misc/aarch64.h"
#define disasm disasm_aa64
#define GPFSEL1         ((volatile unsigned int*)(MMIO_BASE+0x00200004))
#define GPPUD           ((volatile unsigned int*)(MMIO_BASE+0x00200094))
#define GPPUDCLK0       ((volatile unsigned int*)(MMIO_BASE+0x00200098))
#define UART0_DR        ((volatile unsigned int*)(MMIO_BASE+0x00201000))
#define UART0_FR        ((volatile unsigned int*)(MMIO_BASE+0x00201018))
#define UART0_IBRD      ((volatile unsigned int*)(MMIO_BASE+0x00201024))
#define UART0_FBRD      ((volatile unsigned int*)(MMIO_BASE+0x00201028))
#define UART0_LCRH      ((volatile unsigned int*)(MMIO_BASE+0x0020102C))
#define UART0_CR        ((volatile unsigned int*)(MMIO_BASE+0x00201030))
#define UART0_IMSC      ((volatile unsigned int*)(MMIO_BASE+0x00201038))
#define UART0_ICR       ((volatile unsigned int*)(MMIO_BASE+0x00201044))
#define VIDEOCORE_MBOX  (MMIO_BASE+0x0000B880)
#define MBOX_READ       ((volatile unsigned int*)(VIDEOCORE_MBOX+0x0))
#define MBOX_POLL       ((volatile unsigned int*)(VIDEOCORE_MBOX+0x10))
#define MBOX_SENDER     ((volatile unsigned int*)(VIDEOCORE_MBOX+0x14))
#define MBOX_STATUS     ((volatile unsigned int*)(VIDEOCORE_MBOX+0x18))
#define MBOX_CONFIG     ((volatile unsigned int*)(VIDEOCORE_MBOX+0x1C))
#define MBOX_WRITE      ((volatile unsigned int*)(VIDEOCORE_MBOX+0x20))
#define MBOX_RESPONSE   0x80000000
#define MBOX_FULL       0x80000000
#define MBOX_EMPTY      0x40000000
#define MBOX_REQUEST    0
#define MBOX_CH_PROP    8
#define MBOX_TAG_SETCLKRATE     0x38002
#define MBOX_TAG_LAST           0
static unsigned long MMIO_BASE;
volatile unsigned int  __attribute__((aligned(16))) mbox[36];
#endif
#ifdef __x86_64__
#include "../misc/x86_64.h"
#define disasm disasm_x86
char *dbg_regnames[] = { "ax", "bx", "cx", "dx", "si", "di", "sp", "bp" };
char *dbg_exc[] = { "Div zero", "Debug", "NMI", "Breakpoint instruction", "Overflow", "Bound", "Invopcode", "DevUnavail",
    "DblFault", "CoProc", "InvTSS", "SegFault", "StackFault", "GenProt", "PageFault", "Unknown", "Float", "Alignment",
    "MachineCheck", "Double" };
#endif

/**
 * Set baud rate and characteristics (115200 8N1)
 */
static void dbg_uart_init(void)
{
#ifdef __aarch64__
    register uint64_t r;

    /* detect board */
    __asm__ __volatile__ ("mrs %0, midr_el1" : "=r" (r));
    switch(r&0xFFF0) {
        case 0xD030: MMIO_BASE = 0x3F000000; break;     /* Raspberry Pi 3 */
        default:     MMIO_BASE = 0xFE000000; break;     /* Raspberry Pi 4 */
    }

    /* initialize UART */
    *UART0_CR = 0;

    mbox[0] = 9*4;
    mbox[1] = MBOX_REQUEST;
    mbox[2] = MBOX_TAG_SETCLKRATE;
    mbox[3] = 12;
    mbox[4] = 8;
    mbox[5] = 2;           /* UART clock */
    mbox[6] = 4000000;     /* 4Mhz */
    mbox[7] = 0;           /* clear turbo */
    mbox[8] = MBOX_TAG_LAST;
    r = (((unsigned int)((unsigned long)&mbox)&~0xF) | MBOX_CH_PROP);
    do{__asm__ __volatile__("nop");}while(*MBOX_STATUS & MBOX_FULL);
    *MBOX_WRITE = r;
    while(1) {
        do{__asm__ __volatile__("nop");}while(*MBOX_STATUS & MBOX_EMPTY);
        if(r == *MBOX_READ) break;
    }

    /* map UART0 to GPIO pins */
    r=*GPFSEL1;
    r&=~((7<<12)|(7<<15));
    r|=(4<<12)|(4<<15);
    *GPFSEL1 = r;
    *GPPUD = 0;
    r=150; while(r--) { __asm__ __volatile__("nop"); }
    *GPPUDCLK0 = (1<<14)|(1<<15);
    r=150; while(r--) { __asm__ __volatile__("nop"); }
    *GPPUDCLK0 = 0;

    *UART0_ICR = 0x7FF;    /* clear interrupts */
    *UART0_IBRD = 2;       /* 115200 baud */
    *UART0_FBRD = 0xB;
    *UART0_LCRH = 0b11<<5; /* 8n1 */
    *UART0_CR = 0x301;     /* enable Tx, Rx, FIFO */
#endif
#ifdef __x86_64__
    __asm__ __volatile__(
        "movw $0x3f9, %%dx;"
        "xorb %%al, %%al;outb %%al, %%dx;"               /* IER int off */
        "movb $0x80, %%al;addb $2,%%dl;outb %%al, %%dx;" /* LCR set divisor mode */
        "movb $1, %%al;subb $3,%%dl;outb %%al, %%dx;"    /* DLL divisor lo 115200 */
        "xorb %%al, %%al;incb %%dl;outb %%al, %%dx;"     /* DLH divisor hi */
        "incb %%dl;outb %%al, %%dx;"                     /* FCR fifo off */
        "movb $0x43, %%al;incb %%dl;outb %%al, %%dx;"    /* LCR 8N1, break on */
        "movb $0x8, %%al;incb %%dl;outb %%al, %%dx;"     /* MCR Aux out 2 */
        "xorb %%al, %%al;subb $4,%%dl;inb %%dx, %%al"    /* clear receiver/transmitter */
    :::"rax","rdx");
#endif
}

/**
 * Send a character
 */
static void dbg_uart_putc(unsigned int c)
{
#ifdef __aarch64__
    do{__asm__ __volatile__("nop");}while(*UART0_FR&0x20);
    *UART0_DR=c;
#endif
#ifdef __x86_64__
    __asm__ __volatile__(
        "movl $10000,%%ecx;movw $0x3fd,%%dx;"
        "1:inb %%dx, %%al;pause;"
        "cmpb $0xff,%%al;je 2f;"
        "dec %%ecx;jz 2f;"
        "andb $0x20,%%al;jz 1b;"
        "subb $5,%%dl;movb %%bl, %%al;outb %%al, %%dx;2:"
    ::"b"(c):"rax","rcx","rdx");
#endif
}

/**
 * Receive a character
 */
static char dbg_uart_getc(void) {
    char r;
#ifdef __aarch64__
    do{__asm__ __volatile__("nop");}while(*UART0_FR&0x10);
    r=(char)(*UART0_DR);
#endif
#ifdef __x86_64__
    __asm__ __volatile__(
        "movw $0x3fd, %%dx;"
        "1: pause; inb %%dx, %%al;"
        "andb $1, %%al;"
        "jz 1b;"
        "subb $5, %%dl;"
        "inb %%dx, %%al;"
    :"=a"(r)::"rdx");
#endif
    return r=='\r'?'\n':r;
}

/**
 * Decode exception cause
 */
static void dbg_decodeexc(unsigned long type)
{
#ifdef __aarch64__
    unsigned char cause=dbg_regs[33]>>26;

    /* print out interruption type */
    switch(type) {
        case 0: dbg_printf("Synchronous"); break;
        case 1: dbg_printf("IRQ"); break;
        case 2: dbg_printf("FIQ"); break;
        case 3: dbg_printf("SError"); break;
    }
    dbg_printf(": ");
    /* decode exception type (some, not all. See ARM DDI0487B_b chapter D10.2.28) */
    switch(cause) {
        case 0b000000: dbg_printf("Unknown"); break;
        case 0b000001: dbg_printf("Trapped WFI/WFE"); break;
        case 0b001110: dbg_printf("Illegal execution"); break;
        case 0b010101: dbg_printf("System call"); break;
        case 0b100000: dbg_printf("Instruction abort, lower EL"); break;
        case 0b100001: dbg_printf("Instruction abort, same EL"); break;
        case 0b100010: dbg_printf("Instruction alignment fault"); break;
        case 0b100100: dbg_printf("Data abort, lower EL"); break;
        case 0b100101: dbg_printf("Data abort, same EL"); break;
        case 0b100110: dbg_printf("Stack alignment fault"); break;
        case 0b101100: dbg_printf("Floating point"); break;
        case 0b110000: dbg_printf("Breakpoint, lower EL"); break;
        case 0b110001: dbg_printf("Breakpoint, same EL"); break;
        case 0b111100: dbg_printf("Breakpoint instruction"); break;
        default: dbg_printf("Unknown %x", cause); break;
    }
    /* decode data abort cause */
    if(cause==0b100100 || cause==0b100101) {
        dbg_printf(", ");
        switch((dbg_regs[33]>>2)&0x3) {
            case 0: dbg_printf("Address size fault"); break;
            case 1: dbg_printf("Translation fault"); break;
            case 2: dbg_printf("Access flag fault"); break;
            case 3: dbg_printf("Permission fault"); break;
        }
        switch(dbg_regs[33]&0x3) {
            case 0: dbg_printf(" at level 0"); break;
            case 1: dbg_printf(" at level 1"); break;
            case 2: dbg_printf(" at level 2"); break;
            case 3: dbg_printf(" at level 3"); break;
        }
    }
    dbg_printf("\n");
    /* if the exception happened in the debugger, we stop to avoid infinite loop */
    if(dbg_running) {
        dbg_printf("Exception in mini debugger!\n"
            "  elr_el1: %x  spsr_el1: %x\n  esr_el1: %x  far_el1: %x\nsctlr_el1: %x  tcr_el1: %x\n",
            dbg_regs[31],dbg_regs[32],dbg_regs[33],dbg_regs[34],dbg_regs[35],dbg_regs[36]);
        while(1);
    }
#endif
#ifdef __x86_64__
    int i;
    if(type > 31)
        dbg_printf("Interrupt %02x: IRQ %d\n", type, type - 32);
    else
        dbg_printf("Exception %02x: %s, code %x\n", type, type < 20 ? dbg_exc[type] : "Unknown", dbg_regs[23]);
    for(i = 0; i < 8; i++) {
        if(i && i%3==0) dbg_printf("\n");
        dbg_printf("r%s: %16x  ",dbg_regnames[i],dbg_regs[i]);
    }
    for(i = 8; i < 16; i++) {
        if(i && i%3==0) dbg_printf("\n");
        if(i<10) dbg_printf(" ");
        dbg_printf("r%d: %16x  ",i,dbg_regs[i]);
    }
    dbg_printf("\n");
    /* if the exception happened in the debugger, we stop to avoid infinite loop */
    if(dbg_running) {
        dbg_printf("Exception in mini debugger!\n"
            "  rip: %x  cr0: %x\n  cr1: %x  cr2: %x\n  cr3: %x  cr4: %x\nflags: %x code: %x\n",
            dbg_regs[16],dbg_regs[17],dbg_regs[18],dbg_regs[19],dbg_regs[20],dbg_regs[21],dbg_regs[22],dbg_regs[23]);
        while(1);
    }
#endif
}

/**
 * Parse register name
 */
static int dbg_getreg(int i, int *reg)
{
#ifdef __aarch64__
    if(dbg_cmd[i]=='x' || dbg_cmd[i]=='r') {
        i++; if(dbg_cmd[i]>='0' && dbg_cmd[i]<='9') { *reg=dbg_cmd[i]-'0'; }
        i++; if(dbg_cmd[i]>='0' && dbg_cmd[i]<='9') { *reg*=10; *reg+=dbg_cmd[i]-'0'; }
    } else
    if(dbg_cmd[i]=='p' && dbg_cmd[i+1]=='c') { i++; *reg = dbg_regs[31] ? 31 : 30; }
#endif
#ifdef __x86_64__
    int j;
    /* gprs */
    if(dbg_cmd[i]=='r' && ((dbg_cmd[i+1]>='a' && dbg_cmd[i+1]<='d') || dbg_cmd[i+1]=='s')) {
        i++;
        for(j = 0; j < 8; j++)
            if(dbg_cmd[i]==dbg_regnames[j][0] && dbg_cmd[i+1]==dbg_regnames[j][1]) { *reg=j; break; }
    } else
    /* r8 - r15 */
    if(dbg_cmd[i]=='r' && dbg_cmd[i+1]>='0' && dbg_cmd[i+1]<='9') {
        i++; *reg=dbg_cmd[i]-'0';
        i++; if(dbg_cmd[i]>='0' && dbg_cmd[i]<='9') { *reg *= 10; *reg += dbg_cmd[i]-'0'; }
        *reg += 8;
    } else
    /* rip */
    if(dbg_cmd[i]=='p' && dbg_cmd[i+1]=='c') { i++; *reg = 16; } else
    if(dbg_cmd[i]=='r' && dbg_cmd[i+1]=='i' && dbg_cmd[i+2]=='p') { i+=2; *reg = 16; } else
    /* cr0 - cr4 */
    if(dbg_cmd[i]=='c' && dbg_cmd[i+1]=='r' && dbg_cmd[i+2]>='0' && dbg_cmd[i+2]<='4') {
        i+=2; *reg=dbg_cmd[i]-'0'+17;
    }
#endif
    return i;
}

/**
 * Dump registers
 */
static void dbg_dumpreg(void)
{
    int i;
#ifdef __aarch64__
    /* general purpose registers x0-x30 */
    for(i=0;i<31;i++) {
        if(i && i%3==0) dbg_printf("\n");
        if(i<10) dbg_printf(" ");
        dbg_printf("x%d: %16x  ",i,dbg_regs[i]);
    }
    /* some system registers */
    dbg_printf("elr_el1: %x  spsr_el1: %x\n  esr_el1: %x  far_el1: %x\nsctlr_el1: %x  tcr_el1: %x\n",
        dbg_regs[31],dbg_regs[32],dbg_regs[33],dbg_regs[34],dbg_regs[35],dbg_regs[36]);
#endif
#ifdef __x86_64__
    /* gprs */
    for(i = 0; i < 8; i++) {
        if(i && i%3==0) dbg_printf("\n");
        dbg_printf("r%s: %16x  ",dbg_regnames[i],dbg_regs[i]);
    }
    /* r8 - r15 */
    for(i = 8; i < 16; i++) {
        if(i && i%3==0) dbg_printf("\n");
        if(i<10) dbg_printf(" ");
        dbg_printf("r%d: %16x  ",i,dbg_regs[i]);
    }
    dbg_printf("\n");
    /* some system registers */
    dbg_printf("  rip: %x  cr0: %x\n  cr1: %x  cr2: %x\n  cr3: %x  cr4: %x\nflags: %x code: %x\n",
        dbg_regs[16],dbg_regs[17],dbg_regs[18],dbg_regs[19],dbg_regs[20],dbg_regs[21],dbg_regs[22],dbg_regs[23]);
#endif
}

/**
 * minimal sprintf implementation
 */
static unsigned int dbg_vsprintf(char *dst, char* fmt, __builtin_va_list args)
{
    long int arg;
    int len, sign, i;
    char *p, *orig=dst, tmpstr[19], pad=' ';

    if(dst==(void*)0 || fmt==(void*)0)
        return 0;

    arg = 0;
    while(*fmt) {
        if(*fmt=='%') {
            fmt++;
            if(*fmt=='%') goto put;
            len=0;
            if(*fmt=='0') pad='0';
            if(*fmt=='-') fmt++;
            while(*fmt>='0' && *fmt<='9') {
                len *= 10;
                len += *fmt-'0';
                fmt++;
            }
            if(*fmt=='l') fmt++;
            if(*fmt=='c') {
                arg = __builtin_va_arg(args, int);
                *dst++ = (char)arg;
                fmt++;
                continue;
            } else
            if(*fmt=='d') {
                arg = __builtin_va_arg(args, int);
                sign=0;
                if((int)arg<0) {
                    arg*=-1;
                    sign++;
                }
                if(arg>99999999999999999L) {
                    arg=99999999999999999L;
                }
                i=18;
                tmpstr[i]=0;
                do {
                    tmpstr[--i]='0'+(arg%10);
                    arg/=10;
                } while(arg!=0 && i>0);
                if(sign) {
                    tmpstr[--i]='-';
                }
                if(len>0 && len<18) {
                    while(i>18-len) {
                        tmpstr[--i]=pad;
                    }
                }
                p=&tmpstr[i];
                goto copystring;
            } else
            if(*fmt=='x') {
                arg = __builtin_va_arg(args, long int);
                i=16;
                tmpstr[i]=0;
                do {
                    char n=arg & 0xf;
                    /* 0-9 => '0'-'9', 10-15 => 'A'-'F' */
                    tmpstr[--i]=n+(n>9?0x37:0x30);
                    arg>>=4;
                } while(arg!=0 && i>0);
                /* padding, only leading zeros */
                if(len>0 && len<=16) {
                    while(i>16-len) {
                        tmpstr[--i]='0';
                    }
                }
                p=&tmpstr[i];
                goto copystring;
            } else
            if(*fmt=='s') {
                p = __builtin_va_arg(args, char*);
copystring:     if(p==(void*)0) {
                    p="(null)";
                }
                while(*p) {
                    *dst++ = *p++;
                }
            }
        } else {
put:        *dst++ = *fmt;
        }
        fmt++;
    }
    *dst=0;
    return dst-orig;
}

/**
 * Variable length arguments
 */
static unsigned int sprintf(char *dst, char* fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    return dbg_vsprintf(dst,fmt,args);
}

/**
 * Display a string
 */
static void dbg_printf(char *fmt, ...)
{
    char tmp[256], *s = (char*)&tmp;
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    dbg_vsprintf(s,fmt,args);
    while(*s) {
        if(*s=='\n')
            dbg_uart_putc('\r');
        dbg_uart_putc(*s++);
    }
}

/**
 * helper to read a line from user. We redefine some control caracters to handle CSI
 * \e[3~ = 1, delete
 * \e[D  = 2, cursor left
 * \e[C  = 3, cursor right
 */
static void dbg_getline()
{
    int i,dbg_cmdidx=0,dbg_cmdlast=0;
    char c;
    dbg_cmd[0]=0;
    dbg_printf("\r> ");
    while((c=dbg_uart_getc())!='\n') {
        /* decode CSI key sequences (some, not all) */
        if(c==27) {
            c=dbg_uart_getc();
            if(c=='[') {
                c=dbg_uart_getc();
                if(c=='C') c=3; else    /* left */
                if(c=='D') c=2; else    /* right */
                if(c=='3') {
                    c=dbg_uart_getc();
                    if(c=='~') c=1;     /* delete */
                }
            }
        }
        /* Backspace */
        if(c==8 || c==127) {
            if(dbg_cmdidx>0) {
                dbg_cmdidx--;
                for(i=dbg_cmdidx;i<dbg_cmdlast;i++) dbg_cmd[i]=dbg_cmd[i+1];
                dbg_cmdlast--;
            }
        } else
        /* Delete */
        if(c==1) {
            if(dbg_cmdidx<dbg_cmdlast) {
                for(i=dbg_cmdidx;i<dbg_cmdlast;i++) dbg_cmd[i]=dbg_cmd[i+1];
                dbg_cmdlast--;
            }
        } else
        /* cursor left */
        if(c==2) {
            if(dbg_cmdidx>0) dbg_cmdidx--;
        } else
        /* cursor right */
        if(c==3) {
            if(dbg_cmdidx<dbg_cmdlast) dbg_cmdidx++;
        } else {
            /* is there a valid character and space to store it? */
            if(c<' ' || dbg_cmdlast >= (int)sizeof(dbg_cmd)-1) {
                continue;
            }
            if(dbg_cmdidx<dbg_cmdlast) {
                for(i=dbg_cmdlast;i>dbg_cmdidx;i--)
                    dbg_cmd[i]=dbg_cmd[i-1];
            }
            dbg_cmdlast++;
            dbg_cmd[dbg_cmdidx++]=c;
        }
        dbg_cmd[dbg_cmdlast]=0;
        /* display prompt and command line, place cursor with CSI code */
        dbg_printf("\r> %s \r\033[%dC",dbg_cmd,dbg_cmdidx+2);
    }
    dbg_printf("\n");
}

/**
 * helper function to parse the command line for arguments
 */
static unsigned long dbg_getoffs(int i)
{
    unsigned long base=0,ret=0;
    int j=0,sign=0;
    /* if starts with a register */
    if(dbg_cmd[i]>='a' && dbg_cmd[i]<='z') {
        i = dbg_getreg(i, &j);
        if(j>=0 && j<37) base=dbg_regs[j];
        i++;
        if(dbg_cmd[i]=='-') { i++; sign++; }
        if(dbg_cmd[i]=='+') i++;
    }
    /* offset part */
    if(dbg_cmd[i]=='0' && dbg_cmd[i+1]=='x') {
        i+=2;
        /* hex value */
        while((dbg_cmd[i]>='0'&&dbg_cmd[i]<='9')||(dbg_cmd[i]>='a'&&dbg_cmd[i]<='f')||(dbg_cmd[i]>='A'&&dbg_cmd[i]<='F')) {
            ret <<= 4;
            if(dbg_cmd[i]>='0' && dbg_cmd[i]<='9') ret += dbg_cmd[i]-'0';
            else if(dbg_cmd[i] >= 'a' && dbg_cmd[i] <= 'f') ret += dbg_cmd[i]-'a'+10;
            else if(dbg_cmd[i] >= 'A' && dbg_cmd[i] <= 'F') ret += dbg_cmd[i]-'A'+10;
            i++;
        }
    } else {
        /* decimal value */
        while(dbg_cmd[i]>='0'&&dbg_cmd[i]<='9'){
            ret *= 10;
            ret += dbg_cmd[i++]-'0';
        }
    }
    /* return base + offset */
    return sign? base-ret : base+ret;
}

/**
 * main loop, get and parse commands
 */
static void dbg_main(unsigned long excnum)
{
    unsigned long os=0, oe=0, a;
    char c;
    char str[64];
    int i, j;

    if(!dbg_running) {
        dbg_uart_init();
        dbg_printf("Mini debugger by bzt\n");
    }
    dbg_decodeexc(excnum);

    dbg_running++;

    /* main debugger loop */
    while(1) {
        /* get command from user */
        dbg_getline();
        /* parse commands */
        if(dbg_cmd[0]==0 || dbg_cmd[0]=='?' || dbg_cmd[0]=='h') {
            dbg_printf("Mini debugger commands:\n"
                "  ?/h\t\tthis help\n"
                "  c\t\tcontinue execution\n"
                "  n\t\tmove to the next instruction\n"
                "  r\t\tdump registers\n"
                "  x [os [oe]]\texamine memory from offset start (os) to offset end (oe)\n"
                "  i [os [oe]]\tdisassemble instructions from offset start to offset end\n");
            continue;
        } else
        /* continue */
        if(dbg_cmd[0]=='c') break;
        /* next instruction */
        if(dbg_cmd[0]=='n') {
            dbg_regs[31]=disasm(dbg_regs[31]?dbg_regs[31]:dbg_regs[30],NULL);
            dbg_cmd[0]='i';
            goto dis;
        } else
        /* dump registers */
        if(dbg_cmd[0]=='r') {
            dbg_dumpreg();
            continue;
        } else
        /* examine or disassemble, commands with arguments */
        if(dbg_cmd[0]=='x' || dbg_cmd[0]=='i') {
            i=1;
            while(dbg_cmd[i]!=0 && dbg_cmd[i]!=' ') i++;
            while(dbg_cmd[i]!=0 && dbg_cmd[i]==' ') i++;
            if(dbg_cmd[i]!=0) {
                os=oe=dbg_getoffs(i);
                while(dbg_cmd[i]!=0 && dbg_cmd[i]!=' ') i++;
                while(dbg_cmd[i]!=0 && dbg_cmd[i]==' ') i++;
                if(dbg_cmd[i]!=0) {
                    oe=dbg_getoffs(i);
                }
            } else {
                /* no arguments, use defaults */
                if(dbg_cmd[0]=='i') {
dis:                os=oe=dbg_regs[31]?dbg_regs[31]:dbg_regs[30];
                } else {
                    os=oe=dbg_regs[29];
                }
            }
            /* do the thing */
            if(dbg_cmd[0]=='i') {
                /* disassemble bytecode */
                while(os<=oe) {
                    a=os;
                    os=disasm(os,str);
                    dbg_printf("%8x:",a);
                    for(j = 32; a < os; a++, j -= 3) {
                        dbg_printf(" %02x",*((unsigned char*)a));
                    }
                    for(; j > 0; j--)
                        dbg_printf(" ");
                    dbg_printf("%s\n",str);
                }
            } else {
                /* dump memory */
                if(oe<=os) oe=os+16;
                for(a=os;a<oe;a+=16) {
                    dbg_printf("%8x: ", a);
                    for(i=0;i<16;i++) {
                        dbg_printf("%2x%s ",*((unsigned char*)(a+i)),i%4==3?" ":"");
                    }
                    for(i=0;i<16;i++) {
                        c=*((unsigned char*)(a+i));
                        dbg_printf("%c",c<32||c>=127?'.':c);
                    }
                    dbg_printf("\n");
                }
            }
            continue;
        } else {
            dbg_printf("ERROR: unknown command.\n");
        }
    }
    dbg_running--;
}

PLG_API void _start(void)
{
    printf("Debugger plugin activated\n");
    /* workaround a Clang bug, it does not recognize inline assembly calls as function being used */
    (void)&dbg_main;
#ifdef __aarch64__
    __asm__ __volatile__ (
    "ldr     x0, =3f\n"
    "msr     vbar_el1, x0\n"
    "b 4f\n"
    /* save registers */
    "1:\n"
    "str     x0, [sp, #-16]!\n"
    "ldr     x0, =0x580+8\n"
    "str     x1, [x0], #8\n"
    "ldr     x1, [sp, #16]\n"
    "str     x1, [x0, #-16]!\n"
    "add     x0, x0, #16\n"
    "str     x2, [x0], #8\n"
    "str     x3, [x0], #8\n"
    "str     x4, [x0], #8\n"
    "str     x5, [x0], #8\n"
    "str     x6, [x0], #8\n"
    "str     x7, [x0], #8\n"
    "str     x8, [x0], #8\n"
    "str     x9, [x0], #8\n"
    "str     x10, [x0], #8\n"
    "str     x11, [x0], #8\n"
    "str     x12, [x0], #8\n"
    "str     x13, [x0], #8\n"
    "str     x14, [x0], #8\n"
    "str     x15, [x0], #8\n"
    "str     x16, [x0], #8\n"
    "str     x17, [x0], #8\n"
    "str     x18, [x0], #8\n"
    "str     x19, [x0], #8\n"
    "str     x20, [x0], #8\n"
    "str     x21, [x0], #8\n"
    "str     x22, [x0], #8\n"
    "str     x23, [x0], #8\n"
    "str     x24, [x0], #8\n"
    "str     x25, [x0], #8\n"
    "str     x26, [x0], #8\n"
    "str     x27, [x0], #8\n"
    "str     x28, [x0], #8\n"
    "str     x29, [x0], #8\n"
    "ldr     x1, [sp, #16]\n"
    "str     x1, [x0], #8\n"
    "mrs     x1, elr_el1\n"
    "str     x1, [x0], #8\n"
    "mrs     x1, spsr_el1\n"
    "str     x1, [x0], #8\n"
    "mrs     x1, esr_el1\n"
    "str     x1, [x0], #8\n"
    "mrs     x1, far_el1\n"
    "str     x1, [x0], #8\n"
    "mrs     x1, sctlr_el1\n"
    "str     x1, [x0], #8\n"
    "mrs     x1, tcr_el1\n"
    "str     x1, [x0], #8\n"
    "ret\n"
    /* load registers */
    "2:\n"
    "ldr     x0, =0x580+8\n"
    "ldr     x1, [x0], #8\n"
    "ldr     x2, [x0], #8\n"
    "ldr     x3, [x0], #8\n"
    "ldr     x4, [x0], #8\n"
    "ldr     x5, [x0], #8\n"
    "ldr     x6, [x0], #8\n"
    "ldr     x7, [x0], #8\n"
    "ldr     x8, [x0], #8\n"
    "ldr     x9, [x0], #8\n"
    "ldr     x10, [x0], #8\n"
    "ldr     x11, [x0], #8\n"
    "ldr     x12, [x0], #8\n"
    "ldr     x13, [x0], #8\n"
    "ldr     x14, [x0], #8\n"
    "ldr     x15, [x0], #8\n"
    "ldr     x16, [x0], #8\n"
    "ldr     x17, [x0], #8\n"
    "ldr     x18, [x0], #8\n"
    "ldr     x19, [x0], #8\n"
    "ldr     x20, [x0], #8\n"
    "ldr     x21, [x0], #8\n"
    "ldr     x22, [x0], #8\n"
    "ldr     x23, [x0], #8\n"
    "ldr     x24, [x0], #8\n"
    "ldr     x25, [x0], #8\n"
    "ldr     x26, [x0], #8\n"
    "ldr     x27, [x0], #8\n"
    "ldr     x28, [x0], #8\n"
    "ldr     x29, [x0], #8\n"
    "ret\n"
    /* ISRs */
    ".align 11\n"
    "3:\n"
    ".align  7\n"
    "str     x30, [sp, #-16]!\n"
    "bl      1b\n"
    "mov     x0, #0\n"
    "bl      dbg_main\n"
    "bl      2b\n"
    "ldr     x30, [x0], #8\n"
    "ldr     x0, [x0]\n"
    "msr     elr_el1, x0\n"
    "ldr     x0, =0x580\n"
    "ldr     x0, [x0]\n"
    "eret\n"
    ".align  7\n"
    "str     x30, [sp, #-16]!\n"
    "bl      1b\n"
    "mov     x0, #1\n"
    "bl      dbg_main\n"
    "bl      2b\n"
    "ldr     x30, [x0], #8\n"
    "ldr     x0, [x0]\n"
    "msr     elr_el1, x0\n"
    "ldr     x0, =0x580\n"
    "ldr     x0, [x0]\n"
    "eret\n"
    ".align  7\n"
    "str     x30, [sp, #-16]!\n"
    "bl      1b\n"
    "mov     x0, #2\n"
    "bl      dbg_main\n"
    "bl      2b\n"
    "ldr     x30, [x0], #8\n"
    "ldr     x0, [x0]\n"
    "msr     elr_el1, x0\n"
    "ldr     x0, =0x580\n"
    "ldr     x0, [x0]\n"
    "eret\n"
    ".align  7\n"
    "str     x30, [sp, #-16]!\n"
    "bl      1b\n"
    "mov     x0, #3\n"
    "bl      dbg_main\n"
    "bl      2b\n"
    "ldr     x30, [x0], #8\n"
    "ldr     x0, [x0]\n"
    "msr     elr_el1, x0\n"
    "ldr     x0, =0x580\n"
    "ldr     x0, [x0]\n"
    "eret\n"
    "4:"
    );
#endif
#ifdef __x86_64__
    __asm__ __volatile__ (
    ".byte 0xe8;.long 0;"                       /* absolute address needed in IDT */
    "1:popq %%rax;"
    "movq %%rax, %%rsi;addq $4f - 1b, %%rsi;"   /* pointer to the code stubs */
    "movq %%rax, %%rdi;addq $5f - 1b, %%rdi;"   /* pointer to IDT */
    "movw $32, %%cx;\n"                         /* we set up 32 entires in IDT */
    "1:movq %%rsi, %%rax;movw $0x8F01, %%ax;shlq $16, %%rax;movw $32, %%ax;shlq $16, %%rax;movw %%si, %%ax;stosq;"
    "movq %%rsi, %%rax;shrq $32, %%rax;stosq;"
    "addq $64, %%rsi;decw %%cx;jnz 1b;"         /* next entry */
    "movq $5f, (0x522);jmp 6f;"
    /* save registers */
    "1:;"
    "movq    %%rax, 0x580 +  0;"
    "movq    %%rbx, 0x580 +  8;"
    "movq    %%rcx, 0x580 + 16;"
    "movq    %%rdx, 0x580 + 24;"
    "movq    %%rsi, 0x580 + 32;"
    "movq    %%rdi, 0x580 + 40;"
    "movq    40(%%rsp), %%rax;"
    "movq    %%rax, 0x580 + 48;"
    "movq    %%rax, 0x580 + 29*8;"
    "movq    %%rbp, 0x580 + 56;"
    "movq    %%r8, 0x580 + 64;"
    "movq    %%r9, 0x580 + 72;"
    "movq    %%r10, 0x580 + 80;"
    "movq    %%r11, 0x580 + 88;"
    "movq    %%r12, 0x580 + 96;"
    "movq    %%r13, 0x580 + 104;"
    "movq    %%r14, 0x580 + 112;"
    "movq    %%r15, 0x580 + 120;"
    "movq    16(%%rsp), %%rax;"
    "movq    %%rax, 0x580 + 128;"
    "movq    %%rax, 0x580 + 31*8;"
    "movq    %%cr0, %%rax;"
    "movq    %%rax, 0x580 + 136;"
    "movq    %%cr0, %%rax;"
    "movq    %%rax, 0x580 + 144;"
    "movq    %%cr2, %%rax;"
    "movq    %%rax, 0x580 + 152;"
    "movq    %%cr3, %%rax;"
    "movq    %%rax, 0x580 + 160;"
    "movq    %%cr4, %%rax;"
    "movq    %%rax, 0x580 + 168;"
    "movq    32(%%rsp), %%rax;"
    "movq    %%rax, 0x580 + 176;"
    "movq    8(%%rsp), %%rax;"
    "movq    %%rax, 0x580 + 184;"
    "ret;"
    /* load registers */
    "2:;"
    "movq    0x580 +  0, %%rax;"
    "movq    0x580 +  8, %%rbx;"
    "movq    0x580 + 16, %%rcx;"
    "movq    0x580 + 24, %%rdx;"
    "movq    0x580 + 32, %%rsi;"
    "movq    0x580 + 40, %%rdi;"
    "movq    0x580 + 56, %%rbp;"
    "movq    0x580 + 64, %%r8;"
    "movq    0x580 + 72, %%r9;"
    "movq    0x580 + 80, %%r10;"
    "movq    0x580 + 88, %%r11;"
    "movq    0x580 + 96, %%r12;"
    "movq    0x580 + 104, %%r13;"
    "movq    0x580 + 112, %%r14;"
    "movq    0x580 + 120, %%r15;"
    "ret;"
    /* ISRs */
    ".balign 64;4:pushq $0;call 1b;addq $8, %%rsp;movq $0, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $1, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $2, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $3, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $4, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $5, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $6, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $7, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;call 1b;addq $8, %%rsp;movq $8, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $9, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;call 1b;addq $10, %%rsp;movq $8, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;call 1b;addq $11, %%rsp;movq $8, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;call 1b;addq $12, %%rsp;movq $8, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;call 1b;addq $13, %%rsp;movq $8, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;call 1b;addq $14, %%rsp;movq $8, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $15, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $16, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;call 1b;addq $17, %%rsp;movq $8, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $18, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $19, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $20, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $21, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $22, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $23, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $24, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $25, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $26, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $27, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $28, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $29, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;call 1b;addq $8, %%rsp;movq $30, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    ".balign 64;pushq $0;call 1b;addq $8, %%rsp;movq $31, %%rdi;call dbg_main;movq 0x580 + 31*8, %%rax;movq %%rax, (%%rsp);call 2b;iretq;"
    /* IDT */
    ".balign 32;5:.space (32*16);6:"
    :::"rax","rcx","rsi","rdi");
#endif
}
