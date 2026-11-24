#ifndef LOGIN_GREETER_APPLICATION_H
#define LOGIN_GREETER_APPLICATION_H

#include <stdbool.h>

#include <glib.h>

/*
 * The login screen: one full-screen layer surface per monitor, with the
 * login form on the monitor the pointer last entered.
 */

struct login_greeter_options {
    const char *login_daemon_socket_path;
    const char *data_directory;

    const char *configuration_override_path; // NULL for the usual places
    const char *style_sheet_override_path;   // NULL for the shipped one
    GPtrArray  *session_entries;             // struct desktop_session_entry, not empty
};

/*
 * Runs until a session has been started, which returns true, or until the
 * greeter cannot continue, which returns false.
 */
bool login_greeter_application_run(const struct login_greeter_options *options);

#endif
