#ifndef WORKSPACE_WINDOW_BAR_APPLICATION_H
#define WORKSPACE_WINDOW_BAR_APPLICATION_H

#include <stdbool.h>

/* A workspace window bar on every monitor, kept current from the compositor. */

struct workspace_window_bar_options {
    const char *compositor_socket_path;
    const char *data_directory;

    const char *style_sheet_override_path; // NULL for the shipped one
};

/* Runs until the compositor exits; returns false if it could not start. */
bool workspace_window_bar_application_run(const struct workspace_window_bar_options *options);

#endif
