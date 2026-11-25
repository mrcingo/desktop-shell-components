#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#include "process-window-lookup.h"

/* Enough for any real process tree; guards against loops in a damaged /proc. */
enum {
    DEEPEST_ANCESTRY = 64,
};

/* The fourth field of /proc/PID/stat, after the command name in parentheses, which may itself hold ")". */
static int
parse_parent_process_id(
    const char *stat_text
)
{
    const char *name_end  = strrchr(stat_text, ')');
    int         parent_id = 0;

    if (name_end == NULL) {
        return 0;
    }

    if (sscanf(name_end, ") %*c %d", &parent_id) != 1) {
        return 0;
    }

    return parent_id;
}

int
process_window_lookup_read_parent_process_id(
    const char *proc_directory,
    int         process_id
)
{
    g_autofree char *path     = g_strdup_printf("%s/%d/stat", proc_directory, process_id);
    g_autofree char *contents = NULL;

    if (!g_file_get_contents(path, &contents, NULL, NULL)) {
        return 0;
    }

    return parse_parent_process_id(contents);
}

/* Walking stops at init (1) and at unknown processes (0), whose parents are 0. */
static int
parent_or_zero(
    const char *proc_directory,
    int         process_id
)
{
    if (process_id <= 1) {
        return 0;
    }

    return process_window_lookup_read_parent_process_id(proc_directory, process_id);
}

static const struct compositor_window *
find_window_of_process(
    const struct compositor_window_tree *tree,
    int                                  process_id
)
{
    if (process_id <= 1) {
        return NULL;
    }

    for (guint i = 0; i < tree->all_windows->len; i++) {
        const struct compositor_window *window = g_ptr_array_index(tree->all_windows, i);

        if (window->process_id == process_id) {
            return window;
        }
    }

    return NULL;
}

/* The process itself counts as its own first ancestor. */
static const struct compositor_window *
find_window_of_ancestor(
    const struct compositor_window_tree *tree,
    int                                  process_id,
    const char                          *proc_directory
)
{
    int ancestor_id = process_id;

    for (int depth = 0; depth < DEEPEST_ANCESTRY; depth++, ancestor_id = parent_or_zero(proc_directory, ancestor_id)) {
        const struct compositor_window *window = find_window_of_process(tree, ancestor_id);

        if (window != NULL) {
            return window;
        }
    }

    return NULL;
}

static bool
is_ancestor(
    int         ancestor_id,
    int         process_id,
    const char *proc_directory
)
{
    int current_id = parent_or_zero(proc_directory, process_id);

    for (int depth = 0; depth < DEEPEST_ANCESTRY; depth++) {
        if (current_id == ancestor_id) {
            return true;
        }

        current_id = parent_or_zero(proc_directory, current_id);
    }

    return false;
}

static const struct compositor_window *
find_window_of_descendant(
    const struct compositor_window_tree *tree,
    int                                  process_id,
    const char                          *proc_directory
)
{
    for (guint i = 0; i < tree->all_windows->len; i++) {
        const struct compositor_window *window = g_ptr_array_index(tree->all_windows, i);

        if (is_ancestor(process_id, window->process_id, proc_directory)) {
            return window;
        }
    }

    return NULL;
}

static const struct compositor_window *
find_window_by_name(
    const struct compositor_window_tree *tree,
    const char                          *application_name
)
{
    for (guint i = 0; i < tree->all_windows->len; i++) {
        const struct compositor_window *window = g_ptr_array_index(tree->all_windows, i);

        if (g_ascii_strcasecmp(window->application_name, application_name) == 0) {
            return window;
        }
    }

    return NULL;
}

static const struct compositor_window *
find_window_by_process(
    const struct compositor_window_tree *tree,
    int                                  process_id,
    const char                          *proc_directory
)
{
    if (process_id <= 1) {
        return NULL;
    }

    const struct compositor_window *window = find_window_of_ancestor(tree, process_id, proc_directory);

    if (window != NULL) {
        return window;
    }

    return find_window_of_descendant(tree, process_id, proc_directory);
}

const struct compositor_window *
process_window_lookup_find(
    const struct compositor_window_tree *tree,
    int                                  process_id,
    const char                          *application_name,
    const char                          *proc_directory
)
{
    const struct compositor_window *window = find_window_by_process(tree, process_id, proc_directory);

    if (window != NULL) {
        return window;
    }

    if (application_name == NULL) {
        return NULL;
    }

    return find_window_by_name(tree, application_name);
}
