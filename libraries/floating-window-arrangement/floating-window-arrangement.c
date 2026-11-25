#include <string.h>

#include "floating-window-arrangement.h"

enum {
    CORNER_TOLERANCE    = 2,   // pixels: corners this close count as the same
    MAXIMUM_STEPS       = 200, // a cascade gives up after this many steps
    TITLE_BAR_GRIP_SIZE = 64,  // pixels of title bar that must be reachable
};

static int
clamp_start(
    int start,
    int length,
    int area_start,
    int area_length
)
{
    return CLAMP(start, area_start, area_start + MAX(0, area_length - length));
}

/* The frame shrunk to the area if needed and moved fully inside it. */
static struct compositor_rectangle
keep_inside(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *area
)
{
    int width  = MIN(frame->width, area->width);
    int height = MIN(frame->height, area->height);

    return (struct compositor_rectangle){
        clamp_start(frame->x, width, area->x, area->width),
        clamp_start(frame->y, height, area->y, area->height),
        width,
        height,
    };
}

static bool
fits_inside(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *area
)
{
    if (frame->width > area->width) {
        return false;
    }

    return frame->height <= area->height;
}

bool
floating_window_arrangement_fit(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *workspace_area,
    struct compositor_rectangle       *fitted
)
{
    if (fits_inside(frame, workspace_area)) {
        return false;
    }

    int width  = MIN(frame->width, workspace_area->width);
    int height = MIN(frame->height, workspace_area->height);

    *fitted = (struct compositor_rectangle){
        workspace_area->x + (workspace_area->width - width) / 2,
        workspace_area->y + (workspace_area->height - height) / 2,
        width,
        height,
    };

    return true;
}

static bool
shares_corner(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *other
)
{
    if (ABS(frame->x - other->x) > CORNER_TOLERANCE) {
        return false;
    }

    return ABS(frame->y - other->y) <= CORNER_TOLERANCE;
}

static bool
shares_corner_with_any(
    const struct compositor_rectangle *frame,
    const GArray                      *other_frames
)
{
    for (guint i = 0; i < other_frames->len; i++) {
        if (shares_corner(frame, &g_array_index(other_frames, struct compositor_rectangle, i))) {
            return true;
        }
    }

    return false;
}

static bool
leaves_area(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *area
)
{
    if (frame->x + frame->width > area->x + area->width) {
        return true;
    }

    return frame->y + frame->height > area->y + area->height;
}

/* One step down and right; past the area's edge it starts again at the area's top-left corner. */
static void
step_once(
    struct compositor_rectangle       *frame,
    const struct compositor_rectangle *area,
    int                                step
)
{
    frame->x += step;
    frame->y += step;

    if (leaves_area(frame, area)) {
        frame->x = area->x;
        frame->y = area->y;
    }
}

/* Steps until no other window shares the corner, or gives up after many steps. */
static void
step_until_corner_is_free(
    struct compositor_rectangle       *frame,
    const struct compositor_rectangle *workspace_area,
    const GArray                      *other_frames,
    int                                step
)
{
    for (int i = 0; i < MAXIMUM_STEPS; i++) {
        step_once(frame, workspace_area, step);

        if (!shares_corner_with_any(frame, other_frames)) {
            return;
        }
    }
}

bool
floating_window_arrangement_cascade(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *workspace_area,
    const GArray                      *other_frames,
    int                                step,
    struct compositor_rectangle       *cascaded
)
{
    *cascaded = *frame;

    if (!shares_corner_with_any(cascaded, other_frames)) {
        return false;
    }

    step_until_corner_is_free(cascaded, workspace_area, other_frames, step);

    return true;
}

static int
overlap_length(
    int first_start,
    int first_length,
    int second_start,
    int second_length
)
{
    return MIN(first_start + first_length, second_start + second_length) - MAX(first_start, second_start);
}

/* The title bar lies fully within the area's height and enough of it within its width. */
static bool
title_bar_is_reachable_in(
    const struct compositor_rectangle *frame,
    int                                title_bar_height,
    const struct compositor_rectangle *area
)
{
    int grip = MIN(TITLE_BAR_GRIP_SIZE, frame->width);

    if (frame->y < area->y) {
        return false;
    }

    if (frame->y + title_bar_height > area->y + area->height) {
        return false;
    }

    return overlap_length(frame->x, frame->width, area->x, area->width) >= grip;
}

static bool
title_bar_is_reachable(
    const struct compositor_rectangle *frame,
    int                                title_bar_height,
    const GArray                      *workspace_areas
)
{
    for (guint i = 0; i < workspace_areas->len; i++) {
        if (title_bar_is_reachable_in(
                frame,
                title_bar_height,
                &g_array_index(workspace_areas, struct compositor_rectangle, i)
            )) {
            return true;
        }
    }

    return false;
}

/* Squared distance from the frame's centre to the nearest point of the area. */
static double
distance_to_area(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *area
)
{
    double centre_x  = frame->x + frame->width / 2.0;
    double centre_y  = frame->y + frame->height / 2.0;
    double nearest_x = CLAMP(centre_x, (double)area->x, (double)(area->x + area->width));
    double nearest_y = CLAMP(centre_y, (double)area->y, (double)(area->y + area->height));

    return (centre_x - nearest_x) * (centre_x - nearest_x) + (centre_y - nearest_y) * (centre_y - nearest_y);
}

static const struct compositor_rectangle *
nearer_area(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *candidate,
    const struct compositor_rectangle *best
)
{
    if (distance_to_area(frame, candidate) < distance_to_area(frame, best)) {
        return candidate;
    }

    return best;
}

static const struct compositor_rectangle *
find_nearest_area(
    const struct compositor_rectangle *frame,
    const GArray                      *workspace_areas
)
{
    const struct compositor_rectangle *nearest = &g_array_index(workspace_areas, struct compositor_rectangle, 0);

    for (guint i = 1; i < workspace_areas->len; i++) {
        const struct compositor_rectangle *area = &g_array_index(workspace_areas, struct compositor_rectangle, i);

        nearest = nearer_area(frame, area, nearest);
    }

    return nearest;
}

bool
floating_window_arrangement_bring_back(
    const struct compositor_rectangle *frame,
    int                                title_bar_height,
    const GArray                      *workspace_areas,
    struct compositor_rectangle       *brought_back
)
{
    if (workspace_areas->len == 0) {
        return false;
    }

    if (title_bar_is_reachable(frame, title_bar_height, workspace_areas)) {
        return false;
    }

    *brought_back = keep_inside(frame, find_nearest_area(frame, workspace_areas));

    return true;
}

static bool
lies_fully_on_area(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *area
)
{
    struct compositor_rectangle kept = keep_inside(frame, area);

    return memcmp(&kept, frame, sizeof(kept)) == 0;
}

static bool
lies_fully_on_some_area(
    const struct compositor_rectangle *frame,
    const GArray                      *workspace_areas
)
{
    for (guint i = 0; i < workspace_areas->len; i++) {
        if (lies_fully_on_area(frame, &g_array_index(workspace_areas, struct compositor_rectangle, i))) {
            return true;
        }
    }

    return false;
}

bool
floating_window_arrangement_bring_fully_on_screen(
    const struct compositor_rectangle *frame,
    const GArray                      *workspace_areas,
    struct compositor_rectangle       *brought_back
)
{
    if (workspace_areas->len == 0) {
        return false;
    }

    if (lies_fully_on_some_area(frame, workspace_areas)) {
        return false;
    }

    *brought_back = keep_inside(frame, find_nearest_area(frame, workspace_areas));

    return true;
}
