#ifndef SINGLE_INSTANCE_LOCK_H
#define SINGLE_INSTANCE_LOCK_H

#include <stdbool.h>

/*
 * Makes sure only one copy of a program runs per Wayland display, so the
 * compositor can start it on every configuration reload. The lock is an
 * flock(2) on a file in $XDG_RUNTIME_DIR; the kernel releases it when the
 * process exits, even after a crash, so it never goes stale.
 */

struct single_instance_lock;

/*
 * Returns NULL if another copy already holds the lock for instance_name on
 * the current $WAYLAND_DISPLAY, or if the lock file cannot be opened.
 */
struct single_instance_lock *single_instance_lock_acquire(const char *instance_name);

void single_instance_lock_release(struct single_instance_lock *lock);

#endif
