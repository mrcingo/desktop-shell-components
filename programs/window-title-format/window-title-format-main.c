#include <stdlib.h>

#include <glib.h>

#include "program-command-line.h"
#include "single-instance-lock.h"
#include "window-title-format-application.h"

static int
run_title_formatting(
    const struct program_command_line_options *command_line
)
{
    const char *compositor_socket_path = g_getenv("SWAYSOCK");

    if (compositor_socket_path == NULL) {
        g_printerr("SWAYSOCK is not set; start the program from the compositor's configuration\n");
        return EXIT_FAILURE;
    }

    const struct window_title_format_options options = {
        .compositor_socket_path      = compositor_socket_path,
        .configuration_override_path = command_line->configuration_override_path,
    };

    return window_title_format_application_run(&options) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* The compositor starts the program on every configuration reload; extra copies leave quietly. */
static int
run_unless_already_running(
    const struct program_command_line_options *command_line
)
{
    struct single_instance_lock *lock = single_instance_lock_acquire("desktop-shell-window-title-format");

    if (lock == NULL) {
        return EXIT_SUCCESS;
    }

    int exit_status = run_title_formatting(command_line);

    single_instance_lock_release(lock);

    return exit_status;
}

int
main(
    int   argument_count,
    char *arguments[]
)
{
    struct program_command_line_options command_line = {0};

    if (!program_command_line_parse(
            &argument_count,
            &arguments,
            "- adds the process ID, or any other format, to window title bars",
            &command_line
        )) {
        return EXIT_FAILURE;
    }

    int exit_status = run_unless_already_running(&command_line);

    program_command_line_options_clear(&command_line);

    return exit_status;
}
