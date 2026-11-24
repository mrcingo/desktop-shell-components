#ifndef DESKTOP_SESSION_ENTRIES_H
#define DESKTOP_SESSION_ENTRIES_H

#include <glib.h>

/*
 * Login sessions described by desktop entries, as installed in
 * /usr/share/wayland-sessions, see the Desktop Entry Specification.
 */

struct desktop_session_entry {
    char *display_name;

    char **command_arguments;     // NULL-terminated argv
    char **environment_variables; // NULL-terminated KEY=VALUE list
};

/*
 * Loads the visible, installed sessions in directory_path, sorted by name.
 * Invalid entries are skipped with a warning. Returns an array of struct
 * desktop_session_entry, released with g_ptr_array_unref().
 */
GPtrArray *desktop_session_entries_load_directory(const char *directory_path);

#endif
