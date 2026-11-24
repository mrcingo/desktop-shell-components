#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include "replaced-executable-restart.h"

/* What the kernel appends to /proc/self/exe once the file is gone. */
static const char deleted_file_suffix[] = " (deleted)";

struct replaced_executable_restart {
    char **arguments;
};

struct replaced_executable_restart *
replaced_executable_restart_create(
    int   argument_count,
    char *arguments[]
)
{
    struct replaced_executable_restart *restart = g_new0(struct replaced_executable_restart, 1);

    restart->arguments = g_new0(char *, (gsize)argument_count + 1);

    for (int i = 0; i < argument_count; i++) {
        restart->arguments[i] = g_strdup(arguments[i]);
    }

    return restart;
}

void
replaced_executable_restart_destroy(
    struct replaced_executable_restart *restart
)
{
    if (restart == NULL) {
        return;
    }

    g_strfreev(restart->arguments);
    g_free(restart);
}

char *
replaced_executable_find_new_path(
    const char *link_target
)
{
    if (!g_str_has_suffix(link_target, deleted_file_suffix)) {
        return NULL;
    }

    return g_strndup(link_target, strlen(link_target) - strlen(deleted_file_suffix));
}

/* NULL while the program runs from its installed file. */
static char *
find_new_path_of_running_program(void)
{
    g_autofree char *link_target = g_file_read_link("/proc/self/exe", NULL);

    if (link_target == NULL) {
        return NULL;
    }

    return replaced_executable_find_new_path(link_target);
}

/* NULL when the program still runs from its installed file or was uninstalled. */
static char *
find_runnable_new_path(void)
{
    g_autofree char *new_path = find_new_path_of_running_program();

    if (new_path == NULL) {
        return NULL;
    }

    if (access(new_path, X_OK) != 0) {
        return NULL;
    }

    return g_steal_pointer(&new_path);
}

void
replaced_executable_restart_if_replaced(
    const struct replaced_executable_restart *restart
)
{
    g_autofree char *new_path = find_runnable_new_path();

    if (new_path == NULL) {
        return;
    }

    g_message("%s was upgraded; restarting it", new_path);
    execv(new_path, restart->arguments);
    g_warning("Cannot restart %s: %s", new_path, g_strerror(errno));
}
