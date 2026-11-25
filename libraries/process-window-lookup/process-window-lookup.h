#ifndef PROCESS_WINDOW_LOOKUP_H
#define PROCESS_WINDOW_LOOKUP_H

#include "compositor-window-tree.h"

/*
 * The window that belongs to a process, such as the one that sent a
 * notification: the process's own window, else the window of its nearest
 * parent (a terminal running a script), else of a child it started, else a
 * window whose application name matches, ignoring case. Names are compared
 * here, never put into compositor commands.
 */

/* process_id may be 0 and application_name NULL. proc_directory is "/proc" except in tests. NULL when nothing matches. */
const struct compositor_window *process_window_lookup_find(
    const struct compositor_window_tree *tree,
    int                                  process_id,
    const char                          *application_name,
    const char                          *proc_directory
);

/* The parent's process ID from /proc, or 0 when unknown. */
int process_window_lookup_read_parent_process_id(const char *proc_directory, int process_id);

#endif
