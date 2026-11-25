#include <stdlib.h>

#include <gio/gio.h>

#include "notification-daemon-application.h"
#include "program-command-line.h"
#include "replaced-executable-restart.h"
#include "single-instance-lock.h"

/* The source tree's shared-data can stand in for the installed one, as in tests. */
static const char *
find_data_directory(void)
{
    const char *overridden_directory = g_getenv("DESKTOP_SHELL_COMPONENTS_DATA_DIRECTORY");

    if (overridden_directory != NULL) {
        return overridden_directory;
    }

    return INSTALLED_DATA_DIRECTORY;
}

/* The daemon decides itself when the bus is gone, instead of GDBus ending the process. */
static GDBusConnection *
connect_to_session_bus(void)
{
    g_autoptr(GError) error = NULL;

    GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

    if (connection == NULL) {
        g_printerr("Cannot connect to the session bus: %s\n", error->message);
        return NULL;
    }

    g_dbus_connection_set_exit_on_close(connection, FALSE);

    return connection;
}

static int
run_daemon(
    const struct program_command_line_options *command_line,
    const struct replaced_executable_restart  *executable_restart
)
{
    g_autoptr(GDBusConnection) session_bus = connect_to_session_bus();

    if (session_bus == NULL) {
        return EXIT_FAILURE;
    }

    const struct notification_daemon_options options = {
        .session_bus                 = session_bus,
        .compositor_socket_path      = g_getenv("SWAYSOCK"),
        .configuration_override_path = command_line->configuration_override_path,
        .data_directory              = find_data_directory(),
        .server_version              = PROJECT_VERSION,
        .executable_restart          = executable_restart,
    };

    return notification_daemon_application_run(&options) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* The compositor starts the program on every configuration reload; extra copies leave quietly. */
static int
run_unless_already_running(
    const struct program_command_line_options *command_line,
    const struct replaced_executable_restart  *executable_restart
)
{
    struct single_instance_lock *lock = single_instance_lock_acquire("desktop-shell-notification-daemon");

    if (lock == NULL) {
        return EXIT_SUCCESS;
    }

    int exit_status = run_daemon(command_line, executable_restart);

    single_instance_lock_release(lock);

    return exit_status;
}

static int
parse_command_line_and_run(
    int                                       argument_count,
    char                                     *arguments[],
    const struct replaced_executable_restart *executable_restart
)
{
    struct program_command_line_options command_line = {0};

    if (!program_command_line_parse(
            &argument_count,
            &arguments,
            "- notification server with sounds, shown in the status bar",
            &command_line
        )) {
        return EXIT_FAILURE;
    }

    int exit_status = run_unless_already_running(&command_line, executable_restart);

    program_command_line_options_clear(&command_line);

    return exit_status;
}

/* The original command line is kept for restarting after an upgrade, before parsing changes it. */
int
main(
    int   argument_count,
    char *arguments[]
)
{
    struct replaced_executable_restart *executable_restart = replaced_executable_restart_create(argument_count, arguments);
    int                                 exit_status = parse_command_line_and_run(argument_count, arguments, executable_restart);

    replaced_executable_restart_destroy(executable_restart);

    return exit_status;
}
