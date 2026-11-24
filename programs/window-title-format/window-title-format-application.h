#ifndef WINDOW_TITLE_FORMAT_APPLICATION_H
#define WINDOW_TITLE_FORMAT_APPLICATION_H

#include <stdbool.h>

/* Keeps every window's title bar text in the configured format. */

struct window_title_format_options {
    const char *compositor_socket_path;
    const char *configuration_override_path; // NULL for the usual places
};

/* Runs until the compositor exits or the titles are turned off; returns false if it could not start. */
bool window_title_format_application_run(const struct window_title_format_options *options);

#endif
