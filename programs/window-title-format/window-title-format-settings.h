#ifndef WINDOW_TITLE_FORMAT_SETTINGS_H
#define WINDOW_TITLE_FORMAT_SETTINGS_H

#include <stdbool.h>

/*
 * Settings from window-title-format.ini. Every one is optional; see
 * window-title-format.ini in the shipped examples for the defaults.
 */

struct window_title_format_settings {
    bool  is_enabled;
    char *title_format; // see window-title-format-command.h; always quotable
};

/* override_path is the --config file, or NULL. */
struct window_title_format_settings *window_title_format_settings_load(const char *override_path);

void window_title_format_settings_destroy(struct window_title_format_settings *settings);

#endif
