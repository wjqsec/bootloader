/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Mutexes: blocking mutual exclusion locks
 *
 * started by Ingo Molnar:
 *
 *  Copyright (C) 2004, 2005, 2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 *
 * This file contains the main data structure and API definitions.
 */
#ifndef __LINUX_MUTEX_H
#define __LINUX_MUTEX_H

#define mutex_init(...)
#define mutex_lock(...)
#define mutex_unlock(...)
#define mutex_lock_nested(...)
#define mutex_unlock_nested(...)
#define mutex_is_locked(...)	0
struct mutex { int i; };

#define DEFINE_MUTEX(obj) struct mutex  __always_unused obj

#endif /* __LINUX_MUTEX_H */
