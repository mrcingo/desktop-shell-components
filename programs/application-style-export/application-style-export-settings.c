#include <glib.h>

#include "application-style-export-settings.h"
#include "program-configuration-file.h"

/* The setting for each target, in the [export] group. */
static const char *const target_keys[APPLICATION_STYLE_EXPORT_TARGET_COUNT] = {
    [APPLICATION_STYLE_EXPORT_TOOLKIT_3]        = "gtk3",
    [APPLICATION_STYLE_EXPORT_TOOLKIT_4]        = "gtk4",
    [APPLICATION_STYLE_EXPORT_TOOLKIT_SETTINGS] = "gtk-settings",
    [APPLICATION_STYLE_EXPORT_DESKTOP_SETTINGS] = "gsettings",
    [APPLICATION_STYLE_EXPORT_LOCK_SCREEN]      = "lock-screen",
    [APPLICATION_STYLE_EXPORT_PROMPTS]          = "prompts",
};

static const char *const color_scheme_choices[] = {"auto", "dark", "light", NULL};

static const struct program_configuration_key known_keys[] = {
    {"export", "enabled"     },
    {"export", "gtk3"        },
    {"export", "gtk4"        },
    {"export", "gtk-settings"},
    {"export", "gsettings"   },
    {"export", "lock-screen" },
    {"export", "prompts"     },
    {"export", "color-scheme"},
    {NULL,     NULL          },
};

static void
read_target_switches(
    struct application_style_export_settings *settings,
    struct program_configuration_file        *configuration
)
{
    for (int target = 0; target < APPLICATION_STYLE_EXPORT_TARGET_COUNT; target++) {
        settings->exports_target[target] = program_configuration_file_read_boolean(
            configuration,
            "export",
            target_keys[target],
            true
        );
    }
}

static void
read_settings(
    struct application_style_export_settings *settings,
    struct program_configuration_file        *configuration
)
{
    settings->is_enabled   = program_configuration_file_read_boolean(configuration, "export", "enabled", true);
    settings->color_scheme = program_configuration_file_read_choice(
        configuration,
        "export",
        "color-scheme",
        color_scheme_choices,
        0
    );

    read_target_switches(settings, configuration);
}

struct application_style_export_settings *
application_style_export_settings_load(
    const char *override_path
)
{
    struct program_configuration_file *configuration = program_configuration_file_load(
        "application-style-export",
        override_path
    );
    struct application_style_export_settings *settings = g_new0(struct application_style_export_settings, 1);

    program_configuration_file_report_unknown_keys(configuration, known_keys);
    read_settings(settings, configuration);
    program_configuration_file_destroy(configuration);

    return settings;
}

void
application_style_export_settings_destroy(
    struct application_style_export_settings *settings
)
{
    g_free(settings);
}
