#ifndef TRAY_CLICK_WINDOW_FOCUS_SETTINGS_H
#define TRAY_CLICK_WINDOW_FOCUS_SETTINGS_H

#include <stdbool.h>

/*
 * Settings from tray-click-window-focus.ini. Every one is optional; see
 * tray-click-window-focus.ini in the shipped examples for the defaults.
 */

struct tray_click_window_focus_settings {
    bool is_enabled;
    int  second_attempt_milliseconds; // 0 for none
};

/* override_path is the --config file, or NULL. */
struct tray_click_window_focus_settings *tray_click_window_focus_settings_load(const char *override_path);

void tray_click_window_focus_settings_destroy(struct tray_click_window_focus_settings *settings);

#endif
