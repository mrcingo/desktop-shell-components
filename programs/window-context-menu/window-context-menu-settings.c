#include <glib.h>

#include "program-configuration-file.h"
#include "window-context-menu-settings.h"

static const struct program_configuration_key known_keys[] = {
    {"menu",       "enabled"        },
    {"appearance", "follow-sway-bar"},
    {"screenshot", "copy"           },
    {"screenshot", "save"           },
    {"screenshot", "folder"         },
    {NULL,         NULL             },
};

static void
read_settings(
    struct window_context_menu_settings *settings,
    struct program_configuration_file   *configuration
)
{
    settings->is_enabled       = program_configuration_file_read_boolean(configuration, "menu", "enabled", true);
    settings->follows_sway_bar = program_configuration_file_read_boolean(configuration, "appearance", "follow-sway-bar", true);
    settings->copies_screenshots = program_configuration_file_read_boolean(configuration, "screenshot", "copy", true);
    settings->saves_screenshots  = program_configuration_file_read_boolean(configuration, "screenshot", "save", true);
    settings->screenshot_folder  = program_configuration_file_read_text(configuration, "screenshot", "folder", "");
}

struct window_context_menu_settings *
window_context_menu_settings_load(
    const char *override_path
)
{
    struct program_configuration_file   *configuration = program_configuration_file_load("window-context-menu", override_path);
    struct window_context_menu_settings *settings      = g_new0(struct window_context_menu_settings, 1);

    program_configuration_file_report_unknown_keys(configuration, known_keys);
    read_settings(settings, configuration);

    program_configuration_file_destroy(configuration);

    return settings;
}

void
window_context_menu_settings_destroy(
    struct window_context_menu_settings *settings
)
{
    if (settings == NULL) {
        return;
    }

    g_free(settings->screenshot_folder);
    g_free(settings);
}
