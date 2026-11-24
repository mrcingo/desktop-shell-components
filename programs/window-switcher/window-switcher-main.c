#include <locale.h>
#include <stdlib.h>

#include <glib.h>

#include "program-command-line.h"
#include "replaced-executable-restart.h"
#include "shared-style-sheets.h"
#include "single-instance-lock.h"
#include "software-rendering-preference.h"
#include "window-switcher-application.h"

static int
run_switcher(
    const struct program_command_line_options *command_line,
    const struct replaced_executable_restart  *executable_restart
)
{
    const char *compositor_socket_path = g_getenv("SWAYSOCK");

    if (compositor_socket_path == NULL) {
        g_printerr("SWAYSOCK is not set; start the switcher from the compositor's configuration\n");
        return EXIT_FAILURE;
    }

    const struct window_switcher_options options = {
        .compositor_socket_path      = compositor_socket_path,
        .data_directory              = shared_style_sheets_resolve_data_directory(INSTALLED_DATA_DIRECTORY),
        .configuration_override_path = command_line->configuration_override_path,
        .style_sheet_override_path   = command_line->style_sheet_override_path,
        .executable_restart          = executable_restart,
    };

    return window_switcher_application_run(&options) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* The compositor starts the program on every configuration reload; extra copies leave quietly. */
static int
run_unless_already_running(
    const struct program_command_line_options *command_line,
    const struct replaced_executable_restart  *executable_restart
)
{
    struct single_instance_lock *lock = single_instance_lock_acquire("desktop-shell-window-switcher");

    if (lock == NULL) {
        return EXIT_SUCCESS;
    }

    int exit_status = run_switcher(command_line, executable_restart);

    single_instance_lock_release(lock);

    return exit_status;
}

/* Names are translated and sorted for the current locale. */
static void
prepare_process_environment(void)
{
    setlocale(LC_ALL, "");
    prefer_software_rendering_for_simple_interface();
}

static int
parse_command_line_and_run(
    int                                       argument_count,
    char                                     *arguments[],
    const struct replaced_executable_restart *executable_restart
)
{
    struct program_command_line_options command_line = {0};

    prepare_process_environment();

    if (!program_command_line_parse(&argument_count, &arguments, "- Alt+Tab window switcher with previews", &command_line)) {
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
