#ifndef WINDOW_SNAP_SETTINGS_H
#define WINDOW_SNAP_SETTINGS_H

#include <stdbool.h>

/*
 * Settings from window-snap.ini. Every one is optional; see window-snap.ini
 * in the shipped examples for the defaults.
 */

struct window_snap_settings {
    bool is_enabled;
    int  edge_zone_size;        // pixels from a screen edge that snap
    int  drag_threshold;        // pixels the pointer moves before a press becomes a drag
    int  floating_border_width; // given to windows that end up floating
    int  tiled_border_width;    // given to windows that end up in the tiling
    int  preview_opacity;       // percent
    bool shows_labels;
    bool follows_sway_bar;
    bool removes_redundant_groups; // tab groups holding only another group
};

/* override_path is the --config file, or NULL. */
struct window_snap_settings *window_snap_settings_load(const char *override_path);

void window_snap_settings_destroy(struct window_snap_settings *settings);

#endif
