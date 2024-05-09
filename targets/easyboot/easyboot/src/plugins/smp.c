/*
 * src/plugins/smp.c
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
 * @brief Implements Symmetric MultiProcessor Support
 */

#include "../loader.h"

EASY_PLUGIN(PLG_T_TAG) {};

#define sleep(n) do { \
        __asm__ __volatile__ ( "rdtsc" : "=a"(a),"=d"(b)); d = (((uint64_t)b << 32UL)|(uint64_t)a) + n*(*((uint64_t*)0x548)); \
            do { __asm__ __volatile__ ( "pause;rdtsc" : "=a"(a),"=d"(b)); c = ((uint64_t)b << 32UL)|(uint64_t)a; } while(c < d); \
    } while(0)
#define send_ipi(a,m,v) do { \
        while(*((volatile uint32_t*)(lapic + 0x300)) & (1 << 12)) __asm__ __volatile__ ("pause" : : : "memory"); \
        *((volatile uint32_t*)(lapic + 0x310)) = (*((volatile uint32_t*)(lapic + 0x310)) & 0x00ffffff) | (a << 24); \
        *((volatile uint32_t*)(lapic + 0x300)) = (*((volatile uint32_t*)(lapic + 0x300)) & m) | v;  \
    } while(0)

PLG_API void _start(void)
{
    uint8_t *tag, *rsdt = NULL;
#ifdef __x86_64__
    uint8_t *p, *ptr, *end, *lapic = NULL, ids[256], bsp;
    uint32_t a, b;
    uint64_t c, d;
    int i, n;
#endif

    /* check if we have the SMP tag (and for x86, get RSDP as well) */
    for(tag = tags_buf + sizeof(multiboot_info_t);
      tag < tags_ptr && ((multiboot_tag_t*)tag)->type != MULTIBOOT_TAG_TYPE_SMP && ((multiboot_tag_t*)tag)->size;
      tag = (uint8_t*)(((uintptr_t)tag + ((multiboot_tag_t*)tag)->size + 7) & ~7))
        switch(((multiboot_tag_t*)tag)->type) {
            case MULTIBOOT_TAG_TYPE_ACPI_OLD: rsdt = (uint8_t*)(uintptr_t)*((uint32_t*)&((multiboot_tag_old_acpi_t *)tag)->rsdp[16]); break;
            case MULTIBOOT_TAG_TYPE_ACPI_NEW: rsdt = (uint8_t*)(uintptr_t)*((uint64_t*)&((multiboot_tag_new_acpi_t *)tag)->rsdp[24]); break;
        }
    if(tag >= tags_ptr || ((multiboot_tag_t*)tag)->type != MULTIBOOT_TAG_TYPE_SMP) return;

    if(verbose) printf("Initializing SMP...\n");
#ifdef __aarch64__
/* Memory layout (only valid when kernel entry isn't zero)
 *    0x508 -   0x510   sctlr
 *    0x510 -   0x518   vbar
 *    0x518 -   0x520   mair
 *    0x520 -   0x528   tcr
 *    0x528 -   0x530   ttbr0
 *    0x530 -   0x538   ttbr1
 *    0x538 -   0x540   kernel entry point (also SMP semaphor)
 *    0x540 -   0x548   tags_buf
 */
    (void)rsdt;
    ((multiboot_tag_smp_t*)tag)->numcores = ((multiboot_tag_smp_t*)tag)->running = 4;
    __asm__ __volatile__(
    "mov x1, x30; bl 1f;1:mov x0, x30;mov x30, x1;add x0, x0, #2f-1b;"
    "mov x1, #0xE0; str x0, [x1], #0;str x0, [x1], #8;str x0, [x1], #16;b 99f;"
    "2:mov x1, #0x1000;"
    "mrs x0, CurrentEL;and x0, x0, #12;"
    "cmp x0, #12;bne 1f;"                           /* are we running at EL3? */
    "mov x0, #0x5b1;msr scr_el3, x0;mov x0, #0x3c9;msr spsr_el3, x0;adr x0, 1f;msr elr_el3, x0;mov x0, #4;msr sp_el2, x1;eret;"
    "1:cmp x0, #4;beq 1f;"                          /* are we running at EL2? */
    "mrs x0,cnthctl_el2;orr x0,x0,#3;msr cnthctl_el2,x0;msr cnthp_ctl_el2,xzr;"         /* enable CNTP */
    "mov x0,#(1 << 31);orr x0,x0,#2;msr hcr_el2,x0;mrs x0,hcr_el2;"                     /* enable Aarch64 at EL1 */
    "mrs x0,midr_el1;mrs x2,mpidr_el1;msr vpidr_el2,x0;msr vmpidr_el2,x2;"              /* initialize virtual MPIDR */
    "mov x0,#0x33FF;msr cptr_el2,x0;msr hstr_el2,xzr;mov x0,#(3<<20);msr cpacr_el1,x0;" /* disable coprocessor traps */
    "mov x2,#0x0800;movk x2,#0x30d0,lsl #16;msr sctlr_el1, x2;"                         /* setup SCTLR access */
    "mov x2,#0x3c5;msr spsr_el2,x2;adr x2, 1f;msr elr_el2, x2;mov sp, x1;msr sp_el1, x1;eret;"/* switch to EL1 */
    "1:mov sp, x1;mov x2, #0x500;ldr x0, [x2], #0x10;msr vbar_el1,x0;msr SPSel,#0;"     /* set up exception handlers */
    /* spinlock waiting for the kernel entry address */
    "1:ldr x30, [x2], #0x38;nop;nop;nop;nop;cbz x30, 1b;"
    /* initialize AP */
    "ldr x0, [x2], #0x18;msr mair_el1,x0;"
    "ldr x0, [x2], #0x20;msr tcr_el1,x0;"
    "ldr x0, [x2], #0x28;msr ttbr0_el1,x0;"
    "ldr x0, [x2], #0x30;msr ttbr1_el1,x0;"
    "ldr x0, [x2], #0x08;dsb ish;msr sctlr_el1,x0;isb;"
    /* execute 64-bit kernel (stack: 16 byte aligned, and contains the core's id) */
    "mov sp, #0x80000;mrs x0, mpidr_el1;and x0, x0, #3;lsl x1,x0,#10;sub sp,sp,x1;"     /* stack = 0x80000 - coreid * 1024 */
    "str x0, [sp, #-16]!;ldr x0, =0x36d76289;ldr x1, [x2], #0x40;ret;"                  /* jump to kernel entry */
    "99:":::);
#endif
#ifdef __x86_64__
/* Memory layout (only valid when kernel entry isn't zero)
 *    0x510 -   0x520   GDT value
 *    0x520 -   0x530   IDT value
 *    0x530 -   0x538   page table root
 *    0x538 -   0x540   kernel entry point (also SMP semaphor)
 *    0x540 -   0x548   tags_buf
 *    0x548 -   0x550   CPU clockcycles in 1 msec
 *    0x550 -   0x558   lapic address
 *    0x558 -   0x559   AP is running flag
 */
    memset(ids, 0xFF, sizeof(ids)); n = 0;
    if(rsdt && (rsdt[0]=='X' || rsdt[0]=='R') && rsdt[1]=='S' && rsdt[2]=='D' && rsdt[3]=='T')
        for(ptr = rsdt + 36, end = rsdt + *((uint32_t*)(rsdt + 4)); ptr < end; ptr += rsdt[0] == 'X' ? 8 : 4) {
            p = (uint8_t*)(uintptr_t)(rsdt[0] == 'X' ? *((uint64_t*)ptr) : *((uint32_t*)ptr));
            if(p && p[0] == 'A' && p[1] == 'P' && p[2] == 'I' && p[3] == 'C') {
                lapic = (uint8_t*)(uintptr_t)(*((uint32_t*)(p + 0x24)));
                for(ptr = p + 44, end = p + *((uint32_t*)(p + 4)); ptr < end && ptr[1]; ptr += ptr[1])
                    switch(ptr[0]) {
                        case 0: if((ptr[4] & 1) && ptr[3] != 0xFF) ids[n++] = ptr[3]; break;
                        case 5: lapic = (uint8_t*)(uintptr_t)*((uint64_t*)(ptr + 4)); break;
                    }
                break;
            }
        }
    if(!lapic || n < 2) { printf("ERROR: unable to detect local APIC and CPU cores\n"); return; }
    ((multiboot_tag_smp_t*)tag)->numcores = n;
    *((volatile uint64_t*)0x550) = (uint64_t)lapic;

    /* relocate AP startup code to 0x1F000 */
    __asm__ __volatile__(
    /* relocate code */
    ".byte 0xe8;.long 0;"
    "1:popq %%rsi;addq $1f - 1b, %%rsi;movq $0x1f000, %%rdi;movq $99f - 1f, %%rcx;repnz movsb;jmp 99f;"
    /* do the real mode -> prot mode -> long mode trampoline */
    "1:.code16;cli;cld;xorw %%ax, %%ax;movw %%ax, %%ds;incb (0x558);"
    /* spinlock waiting for the kernel entry address */
    "1:pause;cmpl $0, (0x538);jnz 2f;cmpl $0, (0x53C);jz 1b;2:;"
    /* initialize AP */
    "lgdt (0x510);movl %%cr0, %%eax;orb $1, %%al;movl %%eax, %%cr0;"
    ".code32;ljmp $16,$1f+0x100000;1:;"
    "movw $24, %%ax;movw %%ax, %%ds;"
    "movl (0x530), %%eax;movl %%eax, %%cr3;"
    "movl $0xE0, %%eax;movl %%eax, %%cr4;"
    "movl $0x0C0000080, %%ecx;rdmsr;btsl $8, %%eax;wrmsr;"
    "xorb %%cl, %%cl;orl %%ecx, %%eax;btcl $16, %%eax;btsl $31, %%eax;movl %%eax, %%cr0;"
    "lgdt (0x510);ljmp $32,$1f;"
    ".code64;1:;lgdt (0x510);movw $40, %%ax;movw %%ax, %%ds;movw %%ax, %%es;movw %%ax, %%ss;"
    "lidt (0x520);movq (0x550), %%rbx;"
    /* enable lapic */
    "movl $0x1000000, 0xD0(%%rbx);movl $0xFFFFFFFF, 0xE0(%%rbx);"
    "movl 0xF0(%%rbx), %%eax;orl $0x100, %%eax;movl %%eax,0xF0(%%rbx);"
    "movl $0, 0x80(%%rbx);movl $0, 0x280(%%rbx);movl 0x280(%%rbx), %%eax;movl 0x20(%%rbx), %%eax;"
    "shrl $24, %%eax;andq $0xff, %%rax;movq %%rax, %%rbx;shll $10, %%eax;"
    /* execute 64-bit kernel */
    "movq $0x90000, %%rsp;subq %%rax, %%rsp;movq %%rsp, %%rbp;pushq %%rbx;"             /* stack = 0x90000 - coreid * 1024 */
    "movq (0x530), %%rax;movq %%rax, %%cr3;"                                            /* kick the MMU to flush cache */
    "xorq %%rax, %%rax;movl $0x36d76289, %%eax;movq %%rax, %%rcx;movq %%rax, %%rdi;"    /* set arguments */
    "movq (0x540), %%rbx;movq %%rbx, %%rdx;movq %%rbx, %%rsi;"
    /* execute 64-bit kernel (stack: 8 byte aligned, and contains the core's id) */
    /* note: the entry point's function prologue will push rbp, and after that the stack becomes 16 byte aligned as expected */
    "movq (0x538), %%r8;jmp *%%r8;"                                                     /* jump to kernel entry */
    "99:":::);

    /* enable Local APIC */
    *((volatile uint32_t*)(lapic + 0x0D0)) = (1 << 24);
    *((volatile uint32_t*)(lapic + 0x0E0)) = 0xFFFFFFFF;
    *((volatile uint32_t*)(lapic + 0x0F0)) = *((volatile uint32_t*)(lapic + 0x0F0)) | 0x1FF;
    *((volatile uint32_t*)(lapic + 0x080)) = 0;
    ((multiboot_tag_smp_t*)tag)->bspid = bsp = *((volatile uint32_t*)(lapic + 0x20)) >> 24;
    /* initialize APs */
    for(i = 0; i < n; i++) {
        if(ids[i] == bsp) continue;
        *((volatile uint32_t*)(lapic + 0x280)) = 0;                 /* clear APIC errors */
        a = *((volatile uint32_t*)(lapic + 0x280));
        send_ipi(ids[i], 0xfff00000, 0x00C500);                     /* trigger INIT IPI */
        sleep(1);
        send_ipi(ids[i], 0xfff00000, 0x008500);                     /* deassert INIT IPI */
    }
    sleep(10);                                                      /* wait 10 msec */
    /* start APs */
    for(i = 0; i < n; i++) {
        if(ids[i] == bsp) continue;
        *((volatile uint8_t*)0x558) = 0;
        send_ipi(ids[i], 0xfff0f800, 0x00461F);                     /* trigger SIPI, start at 1F00:0000h */
        for(a = 250; !*((volatile uint8_t*)0x558) && a > 0; a--)    /* wait for AP with 250 msec timeout */
            sleep(1);
        if(!*((volatile uint8_t*)0x558)) {
            send_ipi(ids[i], 0xfff0f800, 0x00461F);
            sleep(250);
        }
        if(*((volatile uint8_t*)0x558)) ((multiboot_tag_smp_t*)tag)->running++;
    }
#endif
}
