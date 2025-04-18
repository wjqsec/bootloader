#ifndef __LIB__REAL_H__
#define __LIB__REAL_H__

#include <stdint.h>
#include <stdnoreturn.h>
#include "nyx_api.h"
#define rm_seg(x) ((uint16_t)(((int)x & 0xffff0) >> 4))
#define rm_off(x) ((uint16_t)(((int)x & 0x0000f) >> 0))

#define rm_desegment(seg, off) (((uint32_t)(seg) << 4) + (uint32_t)(off))

#define EFLAGS_CF (1 << 0)
#define EFLAGS_ZF (1 << 6)

struct rm_regs {
    uint16_t gs;
    uint16_t fs;
    uint16_t es;
    uint16_t ds;
    uint32_t eflags;
    uint32_t ebp;
    uint32_t edi;
    uint32_t esi;
    uint32_t edx;
    uint32_t ecx;
    uint32_t ebx;
    uint32_t eax;
} __attribute__((packed));

struct bios_drive_params_ {
    uint16_t buf_size;
    uint16_t info_flags;
    uint32_t cyl;
    uint32_t heads;
    uint32_t sects;
    uint64_t lba_count;
    uint16_t bytes_per_sect;
    uint32_t edd;
} __attribute__((packed));

struct dap_ {
    uint16_t size;
    uint16_t count;
    uint16_t offset;
    uint16_t segment;
    uint64_t lba;
};

void rm_int2(uint8_t int_no, struct rm_regs *out_regs, struct rm_regs *in_regs);
void rm_int(uint8_t int_no, struct rm_regs *out_regs, struct rm_regs *in_regs);

noreturn void rm_hcf(void);

#endif
