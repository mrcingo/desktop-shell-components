#ifndef TRAY_CLICK_WINDOW_FOCUS_APPLICATION_H
#define TRAY_CLICK_WINDOW_FOCUS_APPLICATION_H

#include <stdbool.h>

#include "replaced-executable-restart.h"

/*
 * Brings up an application's window when its tray icon is clicked. sway's
 * bar asks the application to show itself without an activation token, so
 * sway would only mark the window as wanting attention.
 */

struct tray_click_window_focus_options {
    const char *compositor_socket_path;
    const char *configuration_override_path; // NULL for the usual places

    const struct replaced_executable_restart *executable_restart; // used on configuration reloads
};

/* Runs until the compositor exits; returns false if it could not start. */
bool tray_click_window_focus_application_run(const struct tray_click_window_focus_options *options);

#endif
