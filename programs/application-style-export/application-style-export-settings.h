#ifndef APPLICATION_STYLE_EXPORT_SETTINGS_H
#define APPLICATION_STYLE_EXPORT_SETTINGS_H

#include <stdbool.h>

/*
 * Settings from application-style-export.ini. Every one is optional; see
 * application-style-export.ini in the shipped examples for the defaults.
 */

enum application_style_export_target {
    APPLICATION_STYLE_EXPORT_TOOLKIT_3,        // gtk3
    APPLICATION_STYLE_EXPORT_TOOLKIT_4,        // gtk4
    APPLICATION_STYLE_EXPORT_TOOLKIT_SETTINGS, // gtk-settings
    APPLICATION_STYLE_EXPORT_DESKTOP_SETTINGS, // gsettings
    APPLICATION_STYLE_EXPORT_LOCK_SCREEN,      // lock-screen
    APPLICATION_STYLE_EXPORT_PROMPTS,          // prompts
    APPLICATION_STYLE_EXPORT_RECOLORED_ICONS,  // recolor-icons
    APPLICATION_STYLE_EXPORT_TARGET_COUNT,
};

enum application_style_export_color_scheme {
    APPLICATION_STYLE_EXPORT_SCHEME_AUTOMATIC, // dark when the background is dark
    APPLICATION_STYLE_EXPORT_SCHEME_DARK,
    APPLICATION_STYLE_EXPORT_SCHEME_LIGHT,
};

struct application_style_export_settings {
    bool                                       is_enabled;
    bool                                       exports_target[APPLICATION_STYLE_EXPORT_TARGET_COUNT];
    enum application_style_export_color_scheme color_scheme;
    char                                     **recolored_icons; // plain icon names without "-symbolic"; never NULL
};

/* override_path is the --config file, or NULL. */
struct application_style_export_settings *application_style_export_settings_load(const char *override_path);

void application_style_export_settings_destroy(struct application_style_export_settings *settings);

#endif
