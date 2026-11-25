#ifndef WINDOW_CONTEXT_MENU_SETTINGS_H
#define WINDOW_CONTEXT_MENU_SETTINGS_H

#include <stdbool.h>

/*
 * Settings from window-context-menu.ini. Every one is optional; see
 * window-context-menu.ini in the shipped examples for the defaults.
 */

struct window_context_menu_settings {
    bool is_enabled;
    bool follows_sway_bar;

    /* Screenshots: see window-screenshot-capturing.h. */
    bool  copies_screenshots;
    bool  saves_screenshots;
    char *screenshot_folder; // empty for Pictures/Screenshots
};

/* override_path is the --config file, or NULL. */
struct window_context_menu_settings *window_context_menu_settings_load(const char *override_path);

void window_context_menu_settings_destroy(struct window_context_menu_settings *settings);

#endif
