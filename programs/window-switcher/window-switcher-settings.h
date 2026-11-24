#ifndef WINDOW_SWITCHER_SETTINGS_H
#define WINDOW_SWITCHER_SETTINGS_H

#include <stdbool.h>

/*
 * Settings from window-switcher.ini. Every one is optional; see
 * window-switcher.ini in the shipped examples for the defaults.
 */

struct window_switcher_settings {
    bool  includes_all_workspaces; // false: only the focused workspace
    bool  orders_by_recent_focus;  // false: in layout order
    bool  shows_previews;
    int   maximum_preview_width;
    char *label_format; // see window-label-format.h
    bool  follows_sway_bar;
};

/* override_path is the --config file, or NULL. */
struct window_switcher_settings *window_switcher_settings_load(const char *override_path);

void window_switcher_settings_destroy(struct window_switcher_settings *settings);

#endif
