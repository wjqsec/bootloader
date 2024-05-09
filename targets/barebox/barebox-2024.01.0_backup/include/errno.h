/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ERRNO_H
#define __ERRNO_H

#include <linux/errno.h>
#include <linux/err.h>

extern int errno;

void perror(const char *s);
const char *strerror(int errnum);

#endif /* __ERRNO_H */
