#include <glib.h>

#include "program-configuration-file.h"
#include "window-switcher-settings.h"

static const char *const windows_choices[] = {"workspace", "all", NULL};
static const char *const order_choices[]   = {"recent", "layout", NULL};

static const struct program_configuration_key known_keys[] = {
    {"switcher",   "windows"        },
    {"switcher",   "order"          },
    {"switcher",   "previews"       },
    {"switcher",   "preview-width"  },
    {"switcher",   "label"          },
    {"appearance", "follow-sway-bar"},
    {NULL,         NULL             },
};

enum {
    DEFAULT_PREVIEW_WIDTH = 240,
    MINIMUM_PREVIEW_WIDTH = 64,
    MAXIMUM_PREVIEW_WIDTH = 640,
};

static void
read_settings(
    struct window_switcher_settings   *settings,
    struct program_configuration_file *configuration
)
{
    settings->includes_all_workspaces = program_configuration_file_read_choice(
                                            configuration,
                                            "switcher",
                                            "windows",
                                            windows_choices,
                                            0
                                        ) == 1;
    settings->orders_by_recent_focus  = program_configuration_file_read_choice(
                                            configuration,
                                            "switcher",
                                            "order",
                                            order_choices,
                                            0
                                        ) == 0;
    settings->shows_previews          = program_configuration_file_read_boolean(configuration, "switcher", "previews", true);
    settings->maximum_preview_width   = program_configuration_file_read_integer(
        configuration,
        "switcher",
        "preview-width",
        DEFAULT_PREVIEW_WIDTH,
        MINIMUM_PREVIEW_WIDTH,
        MAXIMUM_PREVIEW_WIDTH
    );
    settings->label_format     = program_configuration_file_read_text(configuration, "switcher", "label", "{name} {pid}");
    settings->follows_sway_bar = program_configuration_file_read_boolean(configuration, "appearance", "follow-sway-bar", true);
}

struct window_switcher_settings *
window_switcher_settings_load(
    const char *override_path
)
{
    struct program_configuration_file *configuration = program_configuration_file_load("window-switcher", override_path);
    struct window_switcher_settings   *settings      = g_new0(struct window_switcher_settings, 1);

    program_configuration_file_report_unknown_keys(configuration, known_keys);
    read_settings(settings, configuration);
    program_configuration_file_destroy(configuration);

    return settings;
}

void
window_switcher_settings_destroy(
    struct window_switcher_settings *settings
)
{
    if (settings == NULL) {
        return;
    }

    g_free(settings->label_format);
    g_free(settings);
}
