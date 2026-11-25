#ifndef WINDOW_SNAP_APPLICATION_H
#define WINDOW_SNAP_APPLICATION_H

#include <stdbool.h>

#include "replaced-executable-restart.h"

/*
 * Windows-like title bars. The compositor runs these bindings:
 *
 *   bindsym button1 mark --add _window-snap-target, nop desktop-shell-window-snap press
 *   bindsym --release button1 nop desktop-shell-window-snap release
 *
 * A click focuses the window, as before; a double click maximizes or
 * restores it; a drag shows where the window will land and puts it there.
 * "nop desktop-shell-window-snap left|right|up|down|left-half|right-half|
 * maximize|restore" snaps the focused window from the keyboard.
 */

struct window_snap_options {
    const char *compositor_socket_path;
    const char *data_directory;
    const char *configuration_override_path; // NULL for the usual places
    const char *style_sheet_override_path;   // NULL for the shipped one

    const struct replaced_executable_restart *executable_restart; // used on configuration reloads
};

/* Runs until the compositor exits; returns false if it could not start. */
bool window_snap_application_run(const struct window_snap_options *options);

#endif
