#define _GNU_SOURCE // O_TMPFILE, linkat()

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "exclusive-file-writing.h"

static void
set_error_from_errno(
    GError    **error,
    const char *action,
    const char *subject,
    int         error_number
)
{
    g_set_error(
        error,
        G_IO_ERROR,
        g_io_error_from_errno(error_number),
        "Cannot %s %s: %s",
        action,
        subject,
        g_strerror(error_number)
    );
}

/* The nearest folder of path that exists; path itself when it does. */
static char *
find_existing_ancestor(
    const char *path
)
{
    char *ancestor = g_strdup(path);

    while (!g_file_test(ancestor, G_FILE_TEST_EXISTS)) {
        char *parent = g_path_get_dirname(ancestor);

        g_free(ancestor);

        ancestor = parent;
    }

    return ancestor;
}

static bool
is_inside(
    const char *resolved_path,
    const char *resolved_root
)
{
    if (g_strcmp0(resolved_path, resolved_root) == 0) {
        return true;
    }

    g_autofree char *root_with_separator = g_strconcat(resolved_root, "/", NULL);

    return g_str_has_prefix(resolved_path, root_with_separator);
}

/* NULL with error set when the path cannot be resolved; error is only set once. */
static char *
resolve_path(
    const char *path,
    GError    **error
)
{
    char *resolved_path = realpath(path, NULL);

    if (resolved_path == NULL) {
        g_clear_error(error);
        set_error_from_errno(error, "resolve", path, errno);
    }

    return resolved_path;
}

static bool
check_inside(
    const char *resolved_path,
    const char *resolved_root,
    const char *path,
    const char *allowed_root,
    GError    **error
)
{
    if (is_inside(resolved_path, resolved_root)) {
        return true;
    }

    g_set_error(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED, "Refusing %s: it is outside %s", path, allowed_root);

    return false;
}

/* realpath resolves every symbolic link, so the check holds for where files really go. */
static bool
resolves_inside(
    const char *path,
    const char *allowed_root,
    GError    **error
)
{
    g_autofree char *resolved_path = resolve_path(path, error);
    g_autofree char *resolved_root = resolve_path(allowed_root, error);

    if (resolved_path == NULL) {
        return false;
    }

    if (resolved_root == NULL) {
        return false;
    }

    return check_inside(resolved_path, resolved_root, path, allowed_root, error);
}

static bool
create_folder(
    const char *folder_path,
    GError    **error
)
{
    if (g_mkdir_with_parents(folder_path, 0700) != 0) {
        set_error_from_errno(error, "create", folder_path, errno);
        return false;
    }

    return true;
}

/* Checked before creating anything, and again on the created folder. */
static bool
prepare_folder(
    const char *folder_path,
    const char *allowed_root,
    GError    **error
)
{
    g_autofree char *existing_ancestor = find_existing_ancestor(folder_path);

    if (!resolves_inside(existing_ancestor, allowed_root, error)) {
        return false;
    }

    if (!create_folder(folder_path, error)) {
        return false;
    }

    return resolves_inside(folder_path, allowed_root, error);
}

static int
open_resolved_folder(
    const char *resolved_path,
    GError    **error
)
{
    int folder_descriptor = open(resolved_path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);

    if (folder_descriptor < 0) {
        set_error_from_errno(error, "open", resolved_path, errno);
    }

    return folder_descriptor;
}

int
exclusive_file_writing_open_folder(
    const char *folder_path,
    const char *allowed_root,
    GError    **error
)
{
    if (!prepare_folder(folder_path, allowed_root, error)) {
        return -1;
    }

    g_autofree char *resolved_path = resolve_path(folder_path, error);

    if (resolved_path == NULL) {
        return -1;
    }

    return open_resolved_folder(resolved_path, error);
}

static bool
is_dot_name(
    const char *file_name
)
{
    if (strcmp(file_name, ".") == 0) {
        return true;
    }

    return strcmp(file_name, "..") == 0;
}

static bool
is_plain_file_name(
    const char *file_name
)
{
    if (file_name[0] == '\0') {
        return false;
    }

    if (strchr(file_name, '/') != NULL) {
        return false;
    }

    return !is_dot_name(file_name);
}

static bool
was_interrupted(
    ssize_t written
)
{
    if (written >= 0) {
        return false;
    }

    return errno == EINTR;
}

/* write() returning 0 sets no errno; it means the device took nothing. */
static int
write_failure_errno(
    ssize_t written
)
{
    if (written == 0) {
        return EIO;
    }

    return errno;
}

/* Bytes written, 0 when interrupted, or -1 with error set. */
static ssize_t
write_chunk(
    int           file_descriptor,
    const guint8 *data,
    gsize         length,
    const char   *file_name,
    GError      **error
)
{
    ssize_t written = write(file_descriptor, data, length);

    if (written > 0) {
        return written;
    }

    if (was_interrupted(written)) {
        return 0;
    }

    set_error_from_errno(error, "write", file_name, write_failure_errno(written));

    return -1;
}

static bool
write_bytes(
    int           file_descriptor,
    const guint8 *data,
    gsize         length,
    const char   *file_name,
    GError      **error
)
{
    ssize_t written = 0;

    for (gsize offset = 0; offset < length; offset += (gsize)written) {
        written = write_chunk(file_descriptor, data + offset, length - offset, file_name, error);

        if (written < 0) {
            return false;
        }
    }

    return true;
}

static bool
write_everything(
    int         file_descriptor,
    GBytes     *contents,
    const char *file_name,
    GError    **error
)
{
    gsize         length = 0;
    const guint8 *data   = g_bytes_get_data(contents, &length);

    return write_bytes(file_descriptor, data, length, file_name, error);
}

/* Gives the unnamed file its name; fails with EEXIST rather than replacing a file. */
static bool
link_unnamed_file(
    int         file_descriptor,
    int         folder_descriptor,
    const char *file_name,
    GError    **error
)
{
    g_autofree char *descriptor_path = g_strdup_printf("/proc/self/fd/%d", file_descriptor);

    if (linkat(AT_FDCWD, descriptor_path, folder_descriptor, file_name, AT_SYMLINK_FOLLOW) == 0) {
        return true;
    }

    set_error_from_errno(error, "save", file_name, errno);

    return false;
}

static bool
write_then_link(
    int         file_descriptor,
    int         folder_descriptor,
    const char *file_name,
    GBytes     *contents,
    GError    **error
)
{
    if (!write_everything(file_descriptor, contents, file_name, error)) {
        return false;
    }

    return link_unnamed_file(file_descriptor, folder_descriptor, file_name, error);
}

/* Filesystems without unnamed files, such as some FUSE ones, answer like this. */
static bool
lacks_unnamed_files(
    int error_number
)
{
    if (error_number == EOPNOTSUPP) {
        return true;
    }

    return error_number == EISDIR;
}

static bool
write_and_link_temporary(
    int         file_descriptor,
    int         folder_descriptor,
    const char *temporary_name,
    const char *file_name,
    GBytes     *contents,
    GError    **error
)
{
    if (!write_everything(file_descriptor, contents, file_name, error)) {
        return false;
    }

    if (linkat(folder_descriptor, temporary_name, folder_descriptor, file_name, 0) == 0) {
        return true;
    }

    set_error_from_errno(error, "save", file_name, errno);

    return false;
}

/* Returns the descriptor, or -1 with error set; the name is hidden and unique. */
static int
create_temporary_file(
    int         folder_descriptor,
    const char *temporary_name,
    const char *file_name,
    mode_t      mode,
    GError    **error
)
{
    int file_descriptor = openat(folder_descriptor, temporary_name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, mode);

    if (file_descriptor < 0) {
        set_error_from_errno(error, "create a file for", file_name, errno);
    }

    return file_descriptor;
}

static void
discard_temporary_file(
    int         file_descriptor,
    int         folder_descriptor,
    const char *temporary_name
)
{
    close(file_descriptor);
    unlinkat(folder_descriptor, temporary_name, 0);
}

/* A hidden temporary name, linked to the real name once complete, then removed. */
static bool
write_through_temporary_name(
    int         folder_descriptor,
    const char *file_name,
    GBytes     *contents,
    mode_t      mode,
    GError    **error
)
{
    g_autofree char *temporary_name = g_strdup_printf(".partial-%08x-%s", g_random_int(), file_name);

    int file_descriptor = create_temporary_file(folder_descriptor, temporary_name, file_name, mode, error);

    if (file_descriptor < 0) {
        return false;
    }

    bool succeeded = write_and_link_temporary(file_descriptor, folder_descriptor, temporary_name, file_name, contents, error);

    discard_temporary_file(file_descriptor, folder_descriptor, temporary_name);

    return succeeded;
}

/* Only a filesystem that lacks unnamed files falls back to a temporary name. */
static bool
write_without_unnamed_file(
    int         folder_descriptor,
    const char *file_name,
    GBytes     *contents,
    mode_t      mode,
    GError    **error
)
{
    if (!lacks_unnamed_files(errno)) {
        set_error_from_errno(error, "create a file for", file_name, errno);
        return false;
    }

    return write_through_temporary_name(folder_descriptor, file_name, contents, mode, error);
}

/* An unnamed file (O_TMPFILE) never shows up half written, and vanishes if anything fails. */
static bool
write_through_unnamed_file(
    int         folder_descriptor,
    const char *file_name,
    GBytes     *contents,
    mode_t      mode,
    GError    **error
)
{
    int  file_descriptor = openat(folder_descriptor, ".", O_TMPFILE | O_WRONLY | O_CLOEXEC, mode);
    bool succeeded       = false;

    if (file_descriptor < 0) {
        return write_without_unnamed_file(folder_descriptor, file_name, contents, mode, error);
    }

    succeeded = write_then_link(file_descriptor, folder_descriptor, file_name, contents, error);

    close(file_descriptor);

    return succeeded;
}

bool
exclusive_file_writing_write(
    int         folder_descriptor,
    const char *file_name,
    GBytes     *contents,
    mode_t      mode,
    GError    **error
)
{
    if (!is_plain_file_name(file_name)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME, "Not a plain file name: %s", file_name);
        return false;
    }

    return write_through_unnamed_file(folder_descriptor, file_name, contents, mode, error);
}
