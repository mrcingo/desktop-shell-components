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
    [APPLICATION_STYLE_EXPORT_RECOLORED_ICONS]  = "recolor-icons",
};

/*
 * Side-pane and device icons whose single-color versions Thunar and similar
 * programs load as plain images, in a dark gray that disappears on a dark
 * background (Thunar's Home is go-home, Recent is document-open-recent).
 * Only the single-color versions change, so full-color icons such as the
 * folders in a file list keep their color.
 */
static const char default_recolored_icons[] = "go-home;user-home;user-desktop;user-bookmarks;user-trash;user-trash-full;"
                                              "document-open-recent;starred;folder;folder-documents;folder-download;"
                                              "folder-music;folder-pictures;folder-videos;folder-publicshare;"
                                              "folder-templates;folder-remote;folder-saved-search;network-server;"
                                              "network-workgroup;computer;drive-harddisk;drive-removable-media;"
                                              "drive-optical;media-removable;media-optical;media-flash";

static const char *const color_scheme_choices[] = {"auto", "dark", "light", NULL};

static const struct program_configuration_key known_keys[] = {
    {"export", "enabled"        },
    {"export", "gtk3"           },
    {"export", "gtk4"           },
    {"export", "gtk-settings"   },
    {"export", "gsettings"      },
    {"export", "lock-screen"    },
    {"export", "prompts"        },
    {"export", "color-scheme"   },
    {"export", "icon-theme"     },
    {"icons",  "recolored-icons"},
    {NULL,     NULL             },
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

/* Names may be separated by ";", "," or spaces; odd names are dropped with a warning when the export runs. */
static void
add_if_not_empty(
    GPtrArray  *names,
    const char *piece
)
{
    if (piece[0] != '\0') {
        g_ptr_array_add(names, g_strdup(piece));
    }
}

static char **
read_recolored_icons(
    struct program_configuration_file *configuration
)
{
    g_autofree char
        *text = program_configuration_file_read_text(configuration, "icons", "recolored-icons", default_recolored_icons);

    g_auto(GStrv) pieces = g_strsplit_set(text, ";, \t", -1);

    GPtrArray *names = g_ptr_array_new();

    for (char **piece = pieces; *piece != NULL; piece++) {
        add_if_not_empty(names, *piece);
    }

    g_ptr_array_add(names, NULL);

    return (char **)g_ptr_array_free(names, FALSE);
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

    settings->recolored_icons = read_recolored_icons(configuration);
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
    if (settings == NULL) {
        return;
    }

    g_strfreev(settings->recolored_icons);
    g_free(settings);
}
