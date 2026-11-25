#include <glib.h>

#include "program-configuration-file.h"
#include "window-context-menu-settings.h"

static const struct program_configuration_key known_keys[] = {
    {"menu",       "enabled"        },
    {"appearance", "follow-sway-bar"},
    {NULL,         NULL             },
};

struct window_context_menu_settings *
window_context_menu_settings_load(
    const char *override_path
)
{
    struct program_configuration_file   *configuration = program_configuration_file_load("window-context-menu", override_path);
    struct window_context_menu_settings *settings      = g_new0(struct window_context_menu_settings, 1);

    program_configuration_file_report_unknown_keys(configuration, known_keys);

    settings->is_enabled       = program_configuration_file_read_boolean(configuration, "menu", "enabled", true);
    settings->follows_sway_bar = program_configuration_file_read_boolean(configuration, "appearance", "follow-sway-bar", true);

    program_configuration_file_destroy(configuration);

    return settings;
}

void
window_context_menu_settings_destroy(
    struct window_context_menu_settings *settings
)
{
    g_free(settings);
}
