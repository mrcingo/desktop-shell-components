#include <locale.h>
#include <signal.h>
#include <stdlib.h>

#include <glib.h>

#include "program-command-line.h"
#include "system-status-line-application.h"

/*
 * A closed pipe must end the program through a failed write, not kill it
 * with SIGPIPE; names of days and months follow the user's locale.
 */
static void
prepare_process_environment(void)
{
    setlocale(LC_ALL, "");
    signal(SIGPIPE, SIG_IGN);
}

static int
run_status_line(
    const struct program_command_line_options *command_line
)
{
    const struct system_status_line_options options = {
        .configuration_override_path = command_line->configuration_override_path,
        .compositor_socket_path      = g_getenv("SWAYSOCK"),
    };

    return system_status_line_application_run(&options) ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
main(
    int   argument_count,
    char *arguments[]
)
{
    struct program_command_line_options command_line = {0};

    prepare_process_environment();

    if (!program_command_line_parse(&argument_count, &arguments, "- status line for the compositor's bar", &command_line)) {
        return EXIT_FAILURE;
    }

    int exit_status = run_status_line(&command_line);

    program_command_line_options_clear(&command_line);

    return exit_status;
}
