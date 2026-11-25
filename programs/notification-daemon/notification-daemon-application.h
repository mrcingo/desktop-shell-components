#ifndef NOTIFICATION_DAEMON_APPLICATION_H
#define NOTIFICATION_DAEMON_APPLICATION_H

#include <stdbool.h>

#include <gio/gio.h>

#include "replaced-executable-restart.h"

/*
 * Receives every notification on the session bus, keeps the recent ones,
 * plays their sounds and offers them to the status bar; shows nothing
 * itself.
 */

struct notification_daemon_options {
    GDBusConnection *session_bus;
    const char      *compositor_socket_path;      // NULL without a compositor: windows cannot be focused
    const char      *configuration_override_path; // NULL for the usual places
    const char      *data_directory;              // holds the shipped sounds
    const char      *server_version;

    const struct replaced_executable_restart *executable_restart; // used on configuration reloads; may be NULL
};

/* Runs until the bus or the compositor goes away, or another server owns the name; false if it could not start. */
bool notification_daemon_application_run(const struct notification_daemon_options *options);

#endif
