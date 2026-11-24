#include <locale.h>
#include <stdlib.h>

#include <glib.h>

#include "program-command-line.h"
#include "shared-style-sheets.h"
#include "single-instance-lock.h"
#include "software-rendering-preference.h"
#include "workspace-window-bar-application.h"

static int
run_bar(
    const struct program_command_line_options *command_line
)
{
    const char *compositor_socket_path = g_getenv("SWAYSOCK");

    if (compositor_socket_path == NULL) {
        g_printerr("SWAYSOCK is not set; start the bar from the compositor's configuration\n");
        return EXIT_FAILURE;
    }

    const struct workspace_window_bar_options options = {
        .compositor_socket_path      = compositor_socket_path,
        .data_directory              = shared_style_sheets_resolve_data_directory(INSTALLED_DATA_DIRECTORY),
        .configuration_override_path = command_line->configuration_override_path,
        .style_sheet_override_path   = command_line->style_sheet_override_path,
    };

    return workspace_window_bar_application_run(&options) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* The compositor starts the program on every configuration reload; extra copies leave quietly. */
static int
run_unless_already_running(
    const struct program_command_line_options *command_line
)
{
    struct single_instance_lock *lock = single_instance_lock_acquire("desktop-shell-workspace-window-bar");

    if (lock == NULL) {
        return EXIT_SUCCESS;
    }

    int exit_status = run_bar(command_line);

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

int
main(
    int   argument_count,
    char *arguments[]
)
{
    struct program_command_line_options command_line = {0};

    prepare_process_environment();

    if (!program_command_line_parse(
            &argument_count,
            &arguments,
            "- bar listing the windows of each workspace",
            &command_line
        )) {
        return EXIT_FAILURE;
    }

    int exit_status = run_unless_already_running(&command_line);

    program_command_line_options_clear(&command_line);

    return exit_status;
}
