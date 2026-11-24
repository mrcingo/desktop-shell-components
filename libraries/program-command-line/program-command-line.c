#include <glib.h>

#include "program-command-line.h"

static GOptionContext *
create_option_context(
    const char                          *program_summary,
    struct program_command_line_options *options
)
{
    const GOptionEntry option_entries[] = {
        {
         .long_name       = "style",
         .short_name      = 's',
         .arg             = G_OPTION_ARG_FILENAME,
         .arg_data        = &options->style_sheet_override_path,
         .description     = "Style sheet to use instead of the shipped one",
         .arg_description = "FILE",
         },
        G_OPTION_ENTRY_NULL,
    };

    GOptionContext *context = g_option_context_new(program_summary);

    g_option_context_add_main_entries(context, option_entries, NULL);

    return context;
}

static void
report_parse_error(
    GError *error
)
{
    if (error == NULL) {
        return;
    }

    g_printerr("%s\n", error->message);
    g_error_free(error);
}

bool
program_command_line_parse(
    int                                 *argument_count,
    char                              ***arguments,
    const char                          *program_summary,
    struct program_command_line_options *options
)
{
    GOptionContext *context   = create_option_context(program_summary, options);
    GError         *error     = NULL;
    bool            is_parsed = g_option_context_parse(context, argument_count, arguments, &error);

    report_parse_error(error);
    g_option_context_free(context);

    return is_parsed;
}

void
program_command_line_options_clear(
    struct program_command_line_options *options
)
{
    g_clear_pointer(&options->style_sheet_override_path, g_free);
}
