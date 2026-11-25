#include <glib.h>

#include "program-configuration-file.h"
#include "tray-click-window-focus-settings.h"

enum {
    DEFAULT_SECOND_ATTEMPT_MILLISECONDS = 400,
    LONGEST_SECOND_ATTEMPT_MILLISECONDS = 5000,
};

static const struct program_configuration_key known_keys[] = {
    {"focus", "enabled"       },
    {"focus", "second-attempt"},
    {NULL,    NULL            },
};

struct tray_click_window_focus_settings *
tray_click_window_focus_settings_load(
    const char *override_path
)
{
    struct program_configuration_file *configuration = program_configuration_file_load(
        "tray-click-window-focus",
        override_path
    );
    struct tray_click_window_focus_settings *settings = g_new0(struct tray_click_window_focus_settings, 1);

    program_configuration_file_report_unknown_keys(configuration, known_keys);

    settings->is_enabled                  = program_configuration_file_read_boolean(configuration, "focus", "enabled", true);
    settings->second_attempt_milliseconds = program_configuration_file_read_integer(
        configuration,
        "focus",
        "second-attempt",
        DEFAULT_SECOND_ATTEMPT_MILLISECONDS,
        0,
        LONGEST_SECOND_ATTEMPT_MILLISECONDS
    );

    program_configuration_file_destroy(configuration);

    return settings;
}

void
tray_click_window_focus_settings_destroy(
    struct tray_click_window_focus_settings *settings
)
{
    g_free(settings);
}
