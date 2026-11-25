#ifndef WINDOW_CONTEXT_MENU_TARGET_H
#define WINDOW_CONTEXT_MENU_TARGET_H

#include <stdbool.h>
#include <stdint.h>

#include "compositor-window-tree.h"
#include "process-information-snapshot.h"
#include "process-signal-handle.h"

/*
 * Everything a window menu shows and acts on, taken when the menu opens:
 * the window's details, its process and a handle for signalling exactly
 * that process.
 */

struct window_context_menu_target {
    int64_t container_id;
    char   *title;
    char   *application_name;
    bool    is_floating;
    bool    is_fullscreen;
    bool    is_sticky;
    bool    is_x11_window;
    char   *capture_identifier; // for screenshots; NULL when the compositor offers none

    /* NULL for X11 windows, whose process ID is the X server's, or when the process is gone. */
    struct process_information_snapshot *process;
    struct process_signal_handle        *signal_handle;
};

/* proc_directory is "/proc" except in tests. */
struct window_context_menu_target *
window_context_menu_target_create(const struct compositor_window *window, const char *proc_directory);

void window_context_menu_target_destroy(struct window_context_menu_target *target);

/* "foot  PID 4123  312 MiB", or "foot  X11 window". Free the result with g_free(). */
char *window_context_menu_target_describe_process(const struct window_context_menu_target *target);

#endif
