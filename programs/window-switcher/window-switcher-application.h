#ifndef WINDOW_SWITCHER_APPLICATION_H
#define WINDOW_SWITCHER_APPLICATION_H

#include <stdbool.h>

#include "replaced-executable-restart.h"

/*
 * The Alt+Tab window switcher. It waits for the compositor to run one of
 * these key binding commands:
 *
 *   bindsym Mod1+Tab       nop desktop-shell-window-switcher next
 *   bindsym Mod1+Shift+Tab nop desktop-shell-window-switcher previous
 */

struct window_switcher_options {
    const char *compositor_socket_path;
    const char *data_directory;
    const char *configuration_override_path; // NULL for the usual places
    const char *style_sheet_override_path;   // NULL for the shipped one

    const struct replaced_executable_restart *executable_restart; // used on configuration reloads
};

/* Runs until the compositor exits; returns false if it could not start. */
bool window_switcher_application_run(const struct window_switcher_options *options);

#endif
