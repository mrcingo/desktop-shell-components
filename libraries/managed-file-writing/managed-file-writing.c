#define _GNU_SOURCE // O_NOFOLLOW, O_CLOEXEC

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gio/gio.h>

#include "managed-file-writing.h"

enum {
    LARGEST_READ_FILE = 1 << 20,
    NEW_FOLDER_MODE   = 0755,
    WRITTEN_FILE_MODE = 0644,
    READ_CHUNK_SIZE   = 4096,
};

/* A file as found before writing. */
struct existing_file {
    bool  exists;
    bool  is_symbolic_link;
    char *contents; // NULL unless it exists and was read
};

static void
set_error(
    GError    **error,
    const char *message,
    const char *path
)
{
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, message, path);
}

static bool
has_parent_reference(
    const char *relative_path
)
{
    g_auto(GStrv) components = g_strsplit(relative_path, "/", -1);

    for (char **component = components; *component != NULL; component++) {
        if (strcmp(*component, "..") == 0) {
            return true;
        }
    }

    return false;
}

static bool
is_acceptable_relative_path(
    const char *relative_path
)
{
    if (relative_path[0] == '\0') {
        return false;
    }

    if (g_path_is_absolute(relative_path)) {
        return false;
    }

    return !has_parent_reference(relative_path);
}

static bool
is_same_or_below(
    const char *resolved_path,
    const char *resolved_base
)
{
    if (strcmp(resolved_path, resolved_base) == 0) {
        return true;
    }

    if (!g_str_has_prefix(resolved_path, resolved_base)) {
        return false;
    }

    return resolved_path[strlen(resolved_base)] == '/';
}

/* Whether path, with every link resolved, is base or lies inside it. */
static bool
resolves_inside(
    const char *path,
    const char *base
)
{
    g_autofree char *resolved_path = realpath(path, NULL);
    g_autofree char *resolved_base = realpath(base, NULL);

    if (resolved_path == NULL) {
        return false;
    }

    if (resolved_base == NULL) {
        return false;
    }

    return is_same_or_below(resolved_path, resolved_base);
}

static bool
create_folder_inside(
    const char *folder,
    const char *configuration_directory,
    const char *path,
    GError    **error
)
{
    if (g_mkdir_with_parents(folder, NEW_FOLDER_MODE) != 0) {
        set_error(error, "Cannot create the folder of %s", path);
        return false;
    }

    if (!resolves_inside(folder, configuration_directory)) {
        set_error(error, "%s lies outside the configuration directory through a link; not written", path);
        return false;
    }

    return true;
}

/* The full path when relative_path is acceptable; NULL with error set otherwise. */
static char *
build_checked_path(
    const char *configuration_directory,
    const char *relative_path,
    GError    **error
)
{
    if (!is_acceptable_relative_path(relative_path)) {
        set_error(error, "%s is not a plain path inside the configuration directory", relative_path);
        return NULL;
    }

    return g_build_filename(configuration_directory, relative_path, NULL);
}

/* The full path, with its folder created and checked to lie inside the configuration directory; NULL with error set. */
static char *
prepare_file_path(
    const char *configuration_directory,
    const char *relative_path,
    GError    **error
)
{
    g_autofree char *path = build_checked_path(configuration_directory, relative_path, error);

    if (path == NULL) {
        return NULL;
    }

    g_autofree char *folder = g_path_get_dirname(path);

    if (!create_folder_inside(folder, configuration_directory, path, error)) {
        return NULL;
    }

    return g_steal_pointer(&path);
}

/* Appends one chunk; returns its size, 0 at the end, -1 on an error or once the file is too large. */
static ssize_t
append_chunk(
    GString *contents,
    int      file_descriptor
)
{
    char    chunk[READ_CHUNK_SIZE];
    ssize_t read_size = read(file_descriptor, chunk, sizeof(chunk));

    if (read_size <= 0) {
        return read_size;
    }

    g_string_append_len(contents, chunk, read_size);

    return contents->len > LARGEST_READ_FILE ? -1 : read_size;
}

static char *
finish_reading(
    GString *contents,
    ssize_t  last_read_size
)
{
    if (last_read_size < 0) {
        g_string_free(contents, TRUE);
        return NULL;
    }

    return g_string_free(contents, FALSE);
}

/* NULL on a read error or a file larger than LARGEST_READ_FILE. */
static char *
read_whole_file(
    int file_descriptor
)
{
    GString *contents  = g_string_new(NULL);
    ssize_t  read_size = 1;

    while (read_size > 0) {
        read_size = append_chunk(contents, file_descriptor);
    }

    return finish_reading(contents, read_size);
}

/* O_NOFOLLOW makes a symbolic link fail with ELOOP instead of being followed. */
static struct existing_file
inspect_existing_file(
    const char *path
)
{
    int file_descriptor = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);

    if (file_descriptor < 0) {
        return (struct existing_file){.exists = errno != ENOENT, .is_symbolic_link = errno == ELOOP};
    }

    struct existing_file file = {.exists = true, .contents = read_whole_file(file_descriptor)};

    close(file_descriptor);

    return file;
}

static void
clear_existing_file(
    struct existing_file *file
)
{
    g_clear_pointer(&file->contents, g_free);
}

static bool
starts_with_marker(
    const char *contents,
    const char *marker_line
)
{
    size_t marker_length = strlen(marker_line);

    if (strncmp(contents, marker_line, marker_length) != 0) {
        return false;
    }

    return contents[marker_length] == '\n';
}

/* Ours to replace: absent, or readable and carrying the marker. */
static bool
may_replace(
    const struct existing_file *file,
    const char                 *marker_line
)
{
    if (!file->exists) {
        return true;
    }

    if (file->contents == NULL) {
        return false;
    }

    return starts_with_marker(file->contents, marker_line);
}

static enum managed_file_outcome
replace_contents(
    const char                 *path,
    const struct existing_file *file,
    const char                 *new_contents,
    GError                    **error
)
{
    if (g_strcmp0(file->contents, new_contents) == 0) {
        return MANAGED_FILE_UNCHANGED;
    }

    if (!g_file_set_contents_full(path, new_contents, -1, G_FILE_SET_CONTENTS_CONSISTENT, WRITTEN_FILE_MODE, error)) {
        return MANAGED_FILE_FAILED;
    }

    return MANAGED_FILE_WRITTEN;
}

static enum managed_file_outcome
write_if_ours(
    const char *path,
    const char *marker_line,
    const char *contents,
    GError    **error
)
{
    struct existing_file file = inspect_existing_file(path);

    g_autofree char *new_contents = g_strconcat(marker_line, "\n", contents, NULL);

    enum managed_file_outcome outcome = MANAGED_FILE_LEFT_TO_USER;

    if (may_replace(&file, marker_line)) {
        outcome = replace_contents(path, &file, new_contents, error);
    }

    clear_existing_file(&file);

    return outcome;
}

enum managed_file_outcome
managed_file_write(
    const char *configuration_directory,
    const char *relative_path,
    const char *marker_line,
    const char *contents,
    GError    **error
)
{
    g_autofree char *path = prepare_file_path(configuration_directory, relative_path, error);

    if (path == NULL) {
        return MANAGED_FILE_FAILED;
    }

    return write_if_ours(path, marker_line, contents, error);
}

static GKeyFile *
parse_key_file(
    const char *contents
)
{
    g_autoptr(GKeyFile) key_file = g_key_file_new();

    if (!g_key_file_load_from_data(key_file, contents, -1, G_KEY_FILE_KEEP_COMMENTS, NULL)) {
        return NULL;
    }

    return g_steal_pointer(&key_file);
}

/* A new, empty settings file when there is none; NULL when the file is not ours to change or does not parse. */
static GKeyFile *
load_key_file(
    const struct existing_file *file
)
{
    if (!file->exists) {
        return g_key_file_new();
    }

    if (file->contents == NULL) {
        return NULL;
    }

    return parse_key_file(file->contents);
}

static void
set_values(
    GKeyFile          *key_file,
    const char        *group_name,
    const char *const *keys,
    const char *const *values
)
{
    for (size_t i = 0; keys[i] != NULL; i++) {
        g_key_file_set_value(key_file, group_name, keys[i], values[i]);
    }
}

static enum managed_file_outcome
update_key_file(
    const char           *path,
    struct existing_file *file,
    const char           *group_name,
    const char *const    *keys,
    const char *const    *values,
    GError              **error
)
{
    g_autoptr(GKeyFile) key_file = load_key_file(file);

    if (key_file == NULL) {
        return MANAGED_FILE_LEFT_TO_USER;
    }

    set_values(key_file, group_name, keys, values);

    g_autofree char *new_contents = g_key_file_to_data(key_file, NULL, NULL);

    return replace_contents(path, file, new_contents, error);
}

enum managed_file_outcome
managed_file_set_key_file_values(
    const char        *configuration_directory,
    const char        *relative_path,
    const char        *group_name,
    const char *const *keys,
    const char *const *values,
    GError           **error
)
{
    g_autofree char *path = prepare_file_path(configuration_directory, relative_path, error);

    if (path == NULL) {
        return MANAGED_FILE_FAILED;
    }

    struct existing_file      file    = inspect_existing_file(path);
    enum managed_file_outcome outcome = update_key_file(path, &file, group_name, keys, values, error);

    clear_existing_file(&file);

    return outcome;
}
