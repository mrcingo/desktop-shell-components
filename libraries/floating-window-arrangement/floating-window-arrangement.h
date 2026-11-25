#ifndef FLOATING_WINDOW_ARRANGEMENT_H
#define FLOATING_WINDOW_ARRANGEMENT_H

#include <stdbool.h>

#include <glib.h>

#include "compositor-window-tree.h"

/*
 * Where floating windows go, as on Windows: a new window bigger than the
 * screen is shrunk to fit; a new window opening exactly over another one
 * (every terminal opens centred at the same size) moves down and right in
 * steps, the way Windows cascades; and a window whose title bar cannot be
 * reached on any monitor is brought back. Frames are outer frames, title
 * bar included, in layout pixels.
 */

/* A new window's frame, fitted into the workspace area; false when it already fits. */
bool floating_window_arrangement_fit(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *workspace_area,
    struct compositor_rectangle       *fitted
);

/*
 * A new window's frame moved off the top-left corners of other_frames
 * (struct compositor_rectangle), in steps of step pixels down and right,
 * starting again at the workspace's top-left corner when it would leave the
 * area. False when no other window shares its corner.
 */
bool floating_window_arrangement_cascade(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *workspace_area,
    const GArray                      *other_frames,
    int                                step,
    struct compositor_rectangle       *cascaded
);

/*
 * For a window whose title bar (title_bar_height pixels at the top of frame)
 * is not fully on any of workspace_areas (struct compositor_rectangle) with
 * at least a grip's width: the frame moved, and shrunk if needed, onto the
 * nearest area. False when the title bar can be reached.
 */
bool floating_window_arrangement_bring_back(
    const struct compositor_rectangle *frame,
    int                                title_bar_height,
    const GArray                      *workspace_areas,
    struct compositor_rectangle       *brought_back
);

/*
 * For a window not fully on any of workspace_areas: the frame moved, and
 * shrunk if needed, fully onto the nearest area, as Windows does when a
 * monitor goes away. False when it already lies fully on one.
 */
bool floating_window_arrangement_bring_fully_on_screen(
    const struct compositor_rectangle *frame,
    const GArray                      *workspace_areas,
    struct compositor_rectangle       *brought_back
);

#endif
