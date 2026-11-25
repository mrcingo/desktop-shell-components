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
         .long_name       = "config",
         .short_name      = 'c',
         .arg             = G_OPTION_ARG_FILENAME,
         .arg_data        = &options->configuration_override_path,
         .description     = "Settings file to read instead of the usual places",
         .arg_description = "FILE",
         },
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

/* GOption stores gboolean, which is int-sized; the caller's flags are bool. */
struct flag_value {
    gboolean is_given;
    bool    *destination;
};

static void
add_flag_entries(
    GOptionContext                         *context,
    const struct program_command_line_flag *flags,
    struct flag_value                      *values
)
{
    for (size_t i = 0; flags[i].long_name != NULL; i++) {
        const GOptionEntry entries[] = {
            {.long_name   = flags[i].long_name,
             .arg         = G_OPTION_ARG_NONE,
             .arg_data    = &values[i].is_given,
             .description = flags[i].description},
            G_OPTION_ENTRY_NULL,
        };

        g_option_context_add_main_entries(context, entries, NULL);
    }
}

static size_t
count_flags(
    const struct program_command_line_flag *flags
)
{
    size_t count = 0;

    while (flags[count].long_name != NULL) {
        count++;
    }

    return count;
}

static void
copy_flag_values(
    const struct flag_value *values,
    size_t                   count
)
{
    for (size_t i = 0; i < count; i++) {
        *values[i].destination = values[i].is_given;
    }
}

static struct flag_value *
create_flag_values(
    const struct program_command_line_flag *flags,
    size_t                                  count
)
{
    struct flag_value *values = g_new0(struct flag_value, count + 1);

    for (size_t i = 0; i < count; i++) {
        values[i].destination = flags[i].is_given;
    }

    return values;
}

/* Parses, reports any problem and frees the context. */
static bool
parse_and_free_context(
    GOptionContext *context,
    int            *argument_count,
    char         ***arguments
)
{
    GError *error     = NULL;
    bool    is_parsed = g_option_context_parse(context, argument_count, arguments, &error);

    report_parse_error(error);
    g_option_context_free(context);

    return is_parsed;
}

bool
program_command_line_parse_with_flags(
    int                                    *argument_count,
    char                                 ***arguments,
    const char                             *program_summary,
    struct program_command_line_options    *options,
    const struct program_command_line_flag *flags
)
{
    size_t flag_count = count_flags(flags);

    g_autofree struct flag_value *values = create_flag_values(flags, flag_count);

    GOptionContext *context = create_option_context(program_summary, options);

    add_flag_entries(context, flags, values);

    bool is_parsed = parse_and_free_context(context, argument_count, arguments);

    copy_flag_values(values, flag_count);

    return is_parsed;
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
    g_clear_pointer(&options->configuration_override_path, g_free);
    g_clear_pointer(&options->style_sheet_override_path, g_free);
}
