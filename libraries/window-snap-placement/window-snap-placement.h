#ifndef WINDOW_SNAP_PLACEMENT_H
#define WINDOW_SNAP_PLACEMENT_H

#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "compositor-window-tree.h"

/*
 * Where a dragged window lands, Windows-like: screen edges and corners make
 * it a floating window filling a half or a quarter of the workspace, the
 * top edge maximizes it, the sides of another tiled window put it back in
 * the tiling next to that window, that window's title bar makes the two
 * tabs, and anywhere else a floating window simply moves. Also the
 * compositor command that does it. No GTK: rectangles are layout pixels.
 */

enum window_snap_action {
    WINDOW_SNAP_DOES_NOTHING,
    WINDOW_SNAP_MOVES_FREELY,      // a floating window follows the pointer
    WINDOW_SNAP_FILLS_SCREEN_AREA, // floating, filling a half, quarter or the whole workspace
    WINDOW_SNAP_TILES_BESIDE,      // back in the tiling, next to the target
    WINDOW_SNAP_TABS_WITH,         // back in the tiling, as a tab with the target
};

enum window_snap_side {
    WINDOW_SNAP_SIDE_LEFT,
    WINDOW_SNAP_SIDE_RIGHT,
    WINDOW_SNAP_SIDE_TOP,
    WINDOW_SNAP_SIDE_BOTTOM,
};

/* What is on the output while a window is dragged. */
struct window_snap_scene {
    struct compositor_rectangle     output_area;
    struct compositor_rectangle     workspace_area;
    const struct compositor_window *dragged_window;
    const GPtrArray                *windows; // struct compositor_window on the visible workspace
    int                             edge_zone_size;
    int                             drag_start_x; // where the drag began, in layout pixels
    int                             drag_start_y;
    /*
     * For a snapped or maximized window: the size it had before, which it
     * gets back as soon as it is dragged away, as on Windows. 0 to keep its size.
     */
    int restored_width;
    int restored_height;
};

struct window_snap_placement {
    enum window_snap_action         action;
    enum window_snap_side           side;    // for TILES_BESIDE
    struct compositor_rectangle     preview; // where the window's frame will be
    const struct compositor_window *target;  // for TILES_BESIDE and TABS_WITH, else NULL
    char                           *label;   // "Left half", "Right of Zed"; NULL when nothing happens
    bool                            resizes; // MOVES_FREELY also changes the window's size
};

/* Border widths the placed window gets, as the user's detach gesture gives them. */
struct window_snap_border_widths {
    int floating_border_width;
    int tiled_border_width;
};

/* The window's outer frame: content, borders and title bar. */
struct compositor_rectangle window_snap_frame_of(const struct compositor_window *window);

/* Fills placement for the pointer at x, y; clear it with window_snap_placement_clear. */
void window_snap_placement_find(
    const struct window_snap_scene *scene,
    int                             pointer_x,
    int                             pointer_y,
    struct window_snap_placement   *placement
);

void window_snap_placement_clear(struct window_snap_placement *placement);

/*
 * The compositor command list for the placement, addressed by container ID
 * and made only of fixed words and numbers. NULL when nothing happens.
 * Free with g_free().
 */
char *window_snap_placement_build_command(
    const struct window_snap_placement     *placement,
    const struct compositor_window         *dragged_window,
    const struct window_snap_border_widths *border_widths
);

#endif
