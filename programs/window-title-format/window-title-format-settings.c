#include <glib.h>

#include "program-configuration-file.h"
#include "window-title-format-command.h"
#include "window-title-format-settings.h"

static const char default_title_format[] = "%title <span alpha='60%'>({pid})</span>";

static const struct program_configuration_key known_keys[] = {
    {"titles", "enabled"},
    {"titles", "format" },
    {NULL,     NULL     },
};

/* A format the compositor cannot receive is reported and replaced by the default. */
static char *
read_title_format(
    struct program_configuration_file *configuration
)
{
    char *title_format = program_configuration_file_read_text(configuration, "titles", "format", default_title_format);

    if (window_title_format_can_be_quoted(title_format)) {
        return title_format;
    }

    g_warning(
        "%s: [titles] format uses both quote characters or ends in a backslash; using the default",
        program_configuration_file_get_path(configuration)
    );
    g_free(title_format);

    return g_strdup(default_title_format);
}

struct window_title_format_settings *
window_title_format_settings_load(
    const char *override_path
)
{
    struct program_configuration_file   *configuration = program_configuration_file_load("window-title-format", override_path);
    struct window_title_format_settings *settings      = g_new0(struct window_title_format_settings, 1);

    program_configuration_file_report_unknown_keys(configuration, known_keys);

    settings->is_enabled   = program_configuration_file_read_boolean(configuration, "titles", "enabled", true);
    settings->title_format = read_title_format(configuration);

    program_configuration_file_destroy(configuration);

    return settings;
}

void
window_title_format_settings_destroy(
    struct window_title_format_settings *settings
)
{
    if (settings == NULL) {
        return;
    }

    g_free(settings->title_format);
    g_free(settings);
}
