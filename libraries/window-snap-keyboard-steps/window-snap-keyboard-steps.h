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

#endif
