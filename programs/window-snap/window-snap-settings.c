#include <glib.h>

#include "program-configuration-file.h"
#include "window-snap-settings.h"

static const struct program_configuration_key known_keys[] = {
    {"snap",       "enabled"                },
    {"snap",       "edge-zone-size"         },
    {"snap",       "drag-threshold"         },
    {"snap",       "floating-border-width"  },
    {"snap",       "tiled-border-width"     },
    {"preview",    "opacity"                },
    {"preview",    "show-labels"            },
    {"appearance", "follow-sway-bar"        },
    {"layout",     "remove-redundant-groups"},
    {NULL,         NULL                     },
};

/* A whole number setting: group, key, default, smallest and largest value. */
struct integer_setting {
    const char *group_name;
    const char *key_name;
    int         default_value;
    int         minimum;
    int         maximum;
};

static const struct integer_setting edge_zone_size_setting        = {"snap", "edge-zone-size", 24, 2, 400};
static const struct integer_setting drag_threshold_setting        = {"snap", "drag-threshold", 8, 1, 100};
static const struct integer_setting floating_border_width_setting = {"snap", "floating-border-width", 6, 0, 50};
static const struct integer_setting tiled_border_width_setting    = {"snap", "tiled-border-width", 2, 0, 50};
static const struct integer_setting preview_opacity_setting       = {"preview", "opacity", 30, 0, 100};

static int
read_integer_setting(
    struct program_configuration_file *configuration,
    const struct integer_setting      *setting
)
{
    return program_configuration_file_read_integer(
        configuration,
        setting->group_name,
        setting->key_name,
        setting->default_value,
        setting->minimum,
        setting->maximum
    );
}

static void
read_numbers(
    struct window_snap_settings       *settings,
    struct program_configuration_file *configuration
)
{
    settings->edge_zone_size        = read_integer_setting(configuration, &edge_zone_size_setting);
    settings->drag_threshold        = read_integer_setting(configuration, &drag_threshold_setting);
    settings->floating_border_width = read_integer_setting(configuration, &floating_border_width_setting);
    settings->tiled_border_width    = read_integer_setting(configuration, &tiled_border_width_setting);
    settings->preview_opacity       = read_integer_setting(configuration, &preview_opacity_setting);
}

static void
read_switches(
    struct window_snap_settings       *settings,
    struct program_configuration_file *configuration
)
{
    settings->is_enabled       = program_configuration_file_read_boolean(configuration, "snap", "enabled", true);
    settings->shows_labels     = program_configuration_file_read_boolean(configuration, "preview", "show-labels", true);
    settings->follows_sway_bar = program_configuration_file_read_boolean(configuration, "appearance", "follow-sway-bar", true);
    settings->removes_redundant_groups = program_configuration_file_read_boolean(
        configuration,
        "layout",
        "remove-redundant-groups",
        true
    );
}

struct window_snap_settings *
window_snap_settings_load(
    const char *override_path
)
{
    struct program_configuration_file *configuration = program_configuration_file_load("window-snap", override_path);
    struct window_snap_settings       *settings      = g_new0(struct window_snap_settings, 1);

    program_configuration_file_report_unknown_keys(configuration, known_keys);
    read_numbers(settings, configuration);
    read_switches(settings, configuration);
    program_configuration_file_destroy(configuration);

    return settings;
}

void
window_snap_settings_destroy(
    struct window_snap_settings *settings
)
{
    g_free(settings);
}
