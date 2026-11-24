#include <glib.h>

#include "program-configuration-file.h"
#include "workspace-window-bar-settings.h"

static const char *const position_choices[]     = {"bottom", "top", NULL};
static const char *const click_action_choices[] = {"focus", "close", "nothing", NULL};

static const struct {
    const char                            *key_name;
    enum workspace_window_bar_click_action default_action;
} click_keys[WORKSPACE_WINDOW_BAR_MOUSE_BUTTON_COUNT] = {
    {"left-click",   WORKSPACE_WINDOW_BAR_CLICK_FOCUSES     },
    {"middle-click", WORKSPACE_WINDOW_BAR_CLICK_CLOSES      },
    {"right-click",  WORKSPACE_WINDOW_BAR_CLICK_DOES_NOTHING},
};

static const struct program_configuration_key known_keys[] = {
    {"bar",        "enabled"        },
    {"bar",        "position"       },
    {"bar",        "height"         },
    {"bar",        "label"          },
    {"bar",        "left-click"     },
    {"bar",        "middle-click"   },
    {"bar",        "right-click"    },
    {"appearance", "follow-sway-bar"},
    {NULL,         NULL             },
};

enum {
    MINIMUM_HEIGHT = 10,
    MAXIMUM_HEIGHT = 200,
};

/* "auto", or no value at all, matches sway's top bar. */
static int
read_height(
    struct program_configuration_file *configuration
)
{
    g_autofree char *height_text = program_configuration_file_read_text(configuration, "bar", "height", "auto");

    if (g_strcmp0(height_text, "auto") == 0) {
        return 0;
    }

    return program_configuration_file_read_integer(configuration, "bar", "height", 0, MINIMUM_HEIGHT, MAXIMUM_HEIGHT);
}

static void
read_click_actions(
    struct workspace_window_bar_settings *settings,
    struct program_configuration_file    *configuration
)
{
    for (int button = 0; button < WORKSPACE_WINDOW_BAR_MOUSE_BUTTON_COUNT; button++) {
        settings->click_actions[button] = program_configuration_file_read_choice(
            configuration,
            "bar",
            click_keys[button].key_name,
            click_action_choices,
            click_keys[button].default_action
        );
    }
}

static void
read_settings(
    struct workspace_window_bar_settings *settings,
    struct program_configuration_file    *configuration
)
{
    settings->is_enabled   = program_configuration_file_read_boolean(configuration, "bar", "enabled", true);
    settings->is_at_top    = program_configuration_file_read_choice(configuration, "bar", "position", position_choices, 0) == 1;
    settings->height       = read_height(configuration);
    settings->label_format = program_configuration_file_read_text(configuration, "bar", "label", "{name} {pid}");
    settings->follows_sway_bar = program_configuration_file_read_boolean(configuration, "appearance", "follow-sway-bar", true);

    read_click_actions(settings, configuration);
}

struct workspace_window_bar_settings *
workspace_window_bar_settings_load(
    const char *override_path
)
{
    struct program_configuration_file *configuration = program_configuration_file_load("workspace-window-bar", override_path);
    struct workspace_window_bar_settings *settings   = g_new0(struct workspace_window_bar_settings, 1);

    program_configuration_file_report_unknown_keys(configuration, known_keys);
    read_settings(settings, configuration);
    program_configuration_file_destroy(configuration);

    return settings;
}

void
workspace_window_bar_settings_destroy(
    struct workspace_window_bar_settings *settings
)
{
    if (settings == NULL) {
        return;
    }

    g_free(settings->label_format);
    g_free(settings);
}
