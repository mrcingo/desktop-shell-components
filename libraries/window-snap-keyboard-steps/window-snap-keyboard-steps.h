#ifndef WINDOW_SNAP_KEYBOARD_STEPS_H
#define WINDOW_SNAP_KEYBOARD_STEPS_H

#include <stdbool.h>

#include "compositor-window-tree.h"

/*
 * Snapping with the keyboard as on Windows: Super+Left snaps a window to
 * the left half, Super+Up then makes it the top left quarter, Super+Up on
 * a free window maximizes it and Super+Down restores it. Each window's
 * state decides what the next key does.
 */

enum window_snap_state {
    WINDOW_SNAP_STATE_FREE, // where the user put it
    WINDOW_SNAP_STATE_LEFT_HALF,
    WINDOW_SNAP_STATE_RIGHT_HALF,
    WINDOW_SNAP_STATE_TOP_LEFT_QUARTER,
    WINDOW_SNAP_STATE_TOP_RIGHT_QUARTER,
    WINDOW_SNAP_STATE_BOTTOM_LEFT_QUARTER,
    WINDOW_SNAP_STATE_BOTTOM_RIGHT_QUARTER,
    WINDOW_SNAP_STATE_MAXIMIZED,
    WINDOW_SNAP_STATE_COUNT,
};

enum window_snap_key {
    WINDOW_SNAP_KEY_LEFT,
    WINDOW_SNAP_KEY_RIGHT,
    WINDOW_SNAP_KEY_UP,
    WINDOW_SNAP_KEY_DOWN,
    WINDOW_SNAP_KEY_COUNT,
};

enum window_snap_state window_snap_state_after_key(enum window_snap_state state, enum window_snap_key key);

/* The part of the workspace a snapped state fills; false for the free state. */
bool window_snap_state_area(
    enum window_snap_state             state,
    const struct compositor_rectangle *workspace,
    struct compositor_rectangle       *area
);

/*
 * Reads a keyboard command word: "left", "right", "up" or "down" become a
 * key; "left-half", "right-half", "maximize" and "restore" name a state
 * directly. Returns false for any other word.
 */
bool window_snap_parse_key(const char *word, enum window_snap_key *key);
bool window_snap_parse_state(const char *word, enum window_snap_state *state);

/*
 * Win+Left on a window already against the left edge (the left half or a
 * left quarter) moves it to the monitor on the left, as the same shape on
 * that monitor's right side, as on Windows; Win+Right likewise. Returns
 * false when the key does not cross to another monitor.
 */
bool window_snap_state_crosses_monitors(
    enum window_snap_state  state,
    enum window_snap_key    key,
    enum window_snap_state *state_on_neighbour
);

/*
 * The monitor beside output in the key's direction (left or right): the
 * nearest whose area lies on that side and overlaps it vertically. NULL if
 * there is none, also for the keys up and down.
 */
const struct compositor_output *window_snap_find_neighbour_output(
    const struct compositor_window_tree *tree,
    const struct compositor_output      *output,
    enum window_snap_key                 key
);

/*
 * A frame carried from one workspace area to another: the same size when it
 * fits, else shrunk to fit, at the same relative position, fully inside.
 */
struct compositor_rectangle window_snap_translate_frame(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *from_area,
    const struct compositor_rectangle *to_area
);

/*
 * Whether the frame's top-left corner sits, within a few pixels, on the
 * corner of a snapped state of the workspace area, where keyboard snapping
 * puts windows. A snapped window found elsewhere was moved by the user.
 */
bool
window_snap_frame_is_at_snap_corner(const struct compositor_rectangle *frame, const struct compositor_rectangle *workspace);

#endif
