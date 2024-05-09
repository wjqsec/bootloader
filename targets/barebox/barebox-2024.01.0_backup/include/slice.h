/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __SLICE_H
#define __SLICE_H

#include <bthread.h>
#include <poller.h>

enum slice_action {
	SLICE_ACQUIRE = 1,
	SLICE_RELEASE = -1,
	SLICE_TEST = 0,
};

struct slice {
	int acquired;
	struct list_head deps;
	char *name;
	struct list_head list;
};

struct slice_entry {
	struct slice *slice;
	struct list_head list;
};

void slice_acquire(struct slice *slice);
void slice_release(struct slice *slice);
bool slice_acquired(struct slice *slice);
void slice_depends_on(struct slice *slice, struct slice *dep);
void slice_init(struct slice *slice, const char *name);
void slice_exit(struct slice *slice);

void slice_debug_acquired(struct slice *slice);

extern struct slice command_slice;

void command_slice_acquire(void);
void command_slice_release(void);

#define assert_command_context() do { \
	WARN_ONCE(poller_active(), "%s called in poller\n", __func__); \
} while (0)

#endif
