#ifndef MANAGED_FILE_WRITING_H
#define MANAGED_FILE_WRITING_H

#include <glib.h>

/*
 * Writes files in the user's configuration directory that a program keeps
 * up to date, without ever overwriting what the user wrote:
 *
 *   - a managed file is written only when it does not exist yet or its
 *     first line is the program's marker;
 *   - it is replaced atomically (a temporary file in the same directory,
 *     then a rename), mode 0644, and only when its contents change;
 *   - symbolic links are never written through, and nothing is written
 *     outside the configuration directory, even through linked folders.
 *
 * Nothing here needs, or uses, any privilege.
 */

enum managed_file_outcome {
    MANAGED_FILE_WRITTEN,
    MANAGED_FILE_UNCHANGED,
    MANAGED_FILE_LEFT_TO_USER, // the user's own file, a symbolic link or unreadable settings: not touched
    MANAGED_FILE_FAILED,       // error says why
    MANAGED_FILE_REMOVED,      // only from managed_file_remove
};

/*
 * Writes marker_line, a newline, then contents to relative_path inside
 * configuration_directory, creating missing folders. relative_path must
 * not be absolute or contain "..".
 */
enum managed_file_outcome managed_file_write(
    const char *configuration_directory,
    const char *relative_path,
    const char *marker_line,
    const char *contents,
    GError    **error
);

/*
 * Removes a managed file that is no longer wanted: only when it carries
 * marker_line as its first line and is not a symbolic link. A missing file
 * gives MANAGED_FILE_UNCHANGED, someone else's MANAGED_FILE_LEFT_TO_USER.
 */
enum managed_file_outcome
managed_file_remove(const char *configuration_directory, const char *relative_path, const char *marker_line, GError **error);

/*
 * Sets keys of one group in an INI file (such as a toolkit's settings.ini)
 * and keeps every other line and comment. Works on the user's own file, so
 * no marker is needed; a file that does not parse is left alone.
 * keys and values are NULL-terminated and of equal length.
 */
enum managed_file_outcome managed_file_set_key_file_values(
    const char        *configuration_directory,
    const char        *relative_path,
    const char        *group_name,
    const char *const *keys,
    const char *const *values,
    GError           **error
);

#endif
