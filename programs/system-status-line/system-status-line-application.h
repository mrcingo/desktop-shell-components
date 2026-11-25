#ifndef SYSTEM_STATUS_LINE_APPLICATION_H
#define SYSTEM_STATUS_LINE_APPLICATION_H

#include <stdbool.h>

/*
 * The status command of the compositor's bar: writes the status line
 * protocol to standard output whenever what it shows changes, and reads
 * the bar's click events from standard input.
 */

struct system_status_line_options {
    const char *configuration_override_path; // NULL for the usual places
    const char *compositor_socket_path;      // NULL: no keyboard layout
};

/* Runs until the bar closes standard input or output; returns false if output failed at once. */
bool system_status_line_application_run(const struct system_status_line_options *options);

#endif
