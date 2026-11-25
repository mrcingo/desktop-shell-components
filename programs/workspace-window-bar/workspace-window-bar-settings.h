#ifndef WORKSPACE_WINDOW_BAR_SETTINGS_H
#define WORKSPACE_WINDOW_BAR_SETTINGS_H

#include <stdbool.h>

/*
 * Settings from workspace-window-bar.ini. Every one is optional; see
 * workspace-window-bar.ini in the shipped examples for the defaults.
 */

enum workspace_window_bar_click_action {
    WORKSPACE_WINDOW_BAR_CLICK_FOCUSES,
    WORKSPACE_WINDOW_BAR_CLICK_CLOSES,
    WORKSPACE_WINDOW_BAR_CLICK_DOES_NOTHING,
    WORKSPACE_WINDOW_BAR_CLICK_OPENS_MENU,
};

enum {
    WORKSPACE_WINDOW_BAR_MOUSE_BUTTON_COUNT = 3, // left, middle, right
};

struct workspace_window_bar_settings {
    bool  is_enabled;
    bool  is_at_top;
    int   height;       // 0 matches sway's top bar
    char *label_format; // see window-label-format.h
    bool  follows_sway_bar;

    enum workspace_window_bar_click_action click_actions[WORKSPACE_WINDOW_BAR_MOUSE_BUTTON_COUNT];
};

/* override_path is the --config file, or NULL. */
struct workspace_window_bar_settings *workspace_window_bar_settings_load(const char *override_path);

void workspace_window_bar_settings_destroy(struct workspace_window_bar_settings *settings);

#endif
