/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _ASM_X86_UNALIGNED_H
#define _ASM_X86_UNALIGNED_H

/*
 * The x86 can do unaligned accesses itself.
 */

#include <linux/unaligned/access_ok.h>
#include <linux/unaligned/generic.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define get_unaligned __get_unaligned_le
#define put_unaligned __put_unaligned_le
#else
#define get_unaligned __get_unaligned_be
#define put_unaligned __put_unaligned_be
#endif

#endif /* _ASM_X86_UNALIGNED_H */
