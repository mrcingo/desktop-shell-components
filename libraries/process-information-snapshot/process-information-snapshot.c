#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include "process-information-snapshot.h"

static char *
process_file_path(
    const char *proc_directory,
    int         process_id,
    const char *file_name
)
{
    return g_strdup_printf("%s/%d/%s", proc_directory, process_id, file_name);
}

static void
replace_nul_with_space(
    char *character
)
{
    if (*character == '\0') {
        *character = ' ';
    }
}

/* NULL when the file cannot be read, as for another user's process. */
static char *
read_process_file(
    const char *proc_directory,
    int         process_id,
    const char *file_name,
    gsize      *length
)
{
    g_autofree char *path = process_file_path(proc_directory, process_id, file_name);

    char *contents = NULL;

    if (!g_file_get_contents(path, &contents, length, NULL)) {
        return NULL;
    }

    return contents;
}

/* Arguments are separated by NUL bytes; the result shows them separated by spaces. */
static char *
read_command_line(
    const char *proc_directory,
    int         process_id
)
{
    gsize length = 0;

    g_autofree char *contents = read_process_file(proc_directory, process_id, "cmdline", &length);

    if (contents == NULL) {
        return NULL;
    }

    for (gsize i = 0; i + 1 < length; i++) {
        replace_nul_with_space(&contents[i]);
    }

    return g_utf8_make_valid(contents, (gssize)strlen(contents));
}

static char *
read_working_directory(
    const char *proc_directory,
    int         process_id
)
{
    g_autofree char *path = process_file_path(proc_directory, process_id, "cwd");

    return g_file_read_link(path, NULL);
}

/* statm's second field is the resident size in pages. */
static uint64_t
read_resident_memory_bytes(
    const char *proc_directory,
    int         process_id
)
{
    g_autofree char *contents = read_process_file(proc_directory, process_id, "statm", NULL);

    guint64 pages = 0;

    if (contents == NULL) {
        return 0;
    }

    if (sscanf(contents, "%*u %" G_GUINT64_FORMAT, &pages) != 1) {
        return 0;
    }

    return pages * (uint64_t)sysconf(_SC_PAGESIZE);
}

/* The state letter follows the last ")", as the command name itself may contain one. */
static bool
read_is_stopped(
    const char *proc_directory,
    int         process_id
)
{
    g_autofree char *contents = read_process_file(proc_directory, process_id, "stat", NULL);

    if (contents == NULL) {
        return false;
    }

    const char *name_end = strrchr(contents, ')');

    if (name_end == NULL) {
        return false;
    }

    return strncmp(name_end, ") T", 3) == 0;
}

static bool
process_exists(
    const char *proc_directory,
    int         process_id
)
{
    if (process_id <= 0) {
        return false;
    }

    g_autofree char *path = g_strdup_printf("%s/%d", proc_directory, process_id);

    return g_file_test(path, G_FILE_TEST_IS_DIR);
}

struct process_information_snapshot *
process_information_snapshot_read(
    const char *proc_directory,
    int         process_id
)
{
    if (!process_exists(proc_directory, process_id)) {
        return NULL;
    }

    struct process_information_snapshot *snapshot = g_new0(struct process_information_snapshot, 1);

    *snapshot = (struct process_information_snapshot){
        .process_id            = process_id,
        .command_line          = read_command_line(proc_directory, process_id),
        .working_directory     = read_working_directory(proc_directory, process_id),
        .resident_memory_bytes = read_resident_memory_bytes(proc_directory, process_id),
        .is_stopped            = read_is_stopped(proc_directory, process_id),
    };

    return snapshot;
}

void
process_information_snapshot_destroy(
    struct process_information_snapshot *snapshot
)
{
    if (snapshot == NULL) {
        return;
    }

    g_free(snapshot->command_line);
    g_free(snapshot->working_directory);
    g_free(snapshot);
}

char *
process_information_format_memory_size(
    uint64_t byte_count
)
{
    double mebibytes = (double)byte_count / (1024.0 * 1024.0);

    if (mebibytes < 1024.0) {
        return g_strdup_printf("%.0f MiB", mebibytes);
    }

    return g_strdup_printf("%.1f GiB", mebibytes / 1024.0);
}
