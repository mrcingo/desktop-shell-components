#ifndef WINDOW_CONTEXT_MENU_APPLICATION_H
#define WINDOW_CONTEXT_MENU_APPLICATION_H

#include <stdbool.h>

#include "replaced-executable-restart.h"

/*
 * Opens the window menu when the compositor runs the key or mouse binding
 * "mark --add _window-context-menu-target, nop desktop-shell-window-context-menu":
 * the mark names the window, the nop tells this program.
 */

struct window_context_menu_options {
    const char *compositor_socket_path;
    const char *data_directory;
    const char *configuration_override_path; // NULL for the usual places
    const char *style_sheet_override_path;   // NULL for the shipped one

    const struct replaced_executable_restart *executable_restart; // used on configuration reloads
};

/* Runs until the compositor exits; returns false if it could not start. */
bool window_context_menu_application_run(const struct window_context_menu_options *options);

#endif
