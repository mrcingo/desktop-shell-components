#include <stdlib.h>

#include <glib.h>

#include "application-style-export-application.h"
#include "program-command-line.h"

static int
export_styles(
    const struct program_command_line_options *command_line,
    bool                                       prints_launcher_arguments
)
{
    const struct application_style_export_options options = {
        .compositor_socket_path      = g_getenv("SWAYSOCK"),
        .configuration_directory     = g_get_user_config_dir(),
        .configuration_override_path = command_line->configuration_override_path,
        .prints_launcher_arguments   = prints_launcher_arguments,
    };

    return application_style_export_application_run(&options) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Set by the --print-launcher-arguments option. */
static bool prints_launcher_arguments;

static const struct program_command_line_flag program_flags[] = {
    {"print-launcher-arguments",
     "Print the launcher's color and font options instead of writing files", &prints_launcher_arguments},
    {NULL,                       NULL,                                       NULL                      },
};

int
main(
    int   argument_count,
    char *arguments[]
)
{
    struct program_command_line_options command_line = {0};

    if (!program_command_line_parse_with_flags(
            &argument_count,
            &arguments,
            "- gives other programs the shared look",
            &command_line,
            program_flags
        )) {
        return EXIT_FAILURE;
    }

    int exit_status = export_styles(&command_line, prints_launcher_arguments);

    program_command_line_options_clear(&command_line);

    return exit_status;
}
