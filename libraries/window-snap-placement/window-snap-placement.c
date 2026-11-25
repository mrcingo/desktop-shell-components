#include <inttypes.h>

#include "window-snap-placement.h"

static const char drop_mark[] = "_window-snap-drop";

enum {
    CORNER_ZONE_FACTOR = 4, // corners reach this many edge zones along an edge
};

/* The compositor's window rectangle already holds the borders; only the title bar sits above it. */
struct compositor_rectangle
window_snap_frame_of(
    const struct compositor_window *window
)
{
    return (struct compositor_rectangle){
        window->content_x,
        window->content_y - window->title_bar_height,
        window->content_width,
        window->content_height + window->title_bar_height,
    };
}

static bool
rectangle_contains(
    const struct compositor_rectangle *rectangle,
    int                                x,
    int                                y
)
{
    if (x < rectangle->x || y < rectangle->y) {
        return false;
    }

    return x < rectangle->x + rectangle->width && y < rectangle->y + rectangle->height;
}

/* Screen edges and corners. */

enum screen_edge {
    SCREEN_EDGE_LEFT,
    SCREEN_EDGE_RIGHT,
    SCREEN_EDGE_TOP,
    SCREEN_EDGE_BOTTOM,
    SCREEN_EDGE_COUNT,
};

/* Where along an edge the pointer is: near its start, in the middle or near its end. */
enum edge_stretch {
    EDGE_STRETCH_START,
    EDGE_STRETCH_MIDDLE,
    EDGE_STRETCH_END,
    EDGE_STRETCH_COUNT,
};

/* A part of the workspace in halves: from (x0, y0) to (x1, y1), each 0, 1 or 2. */
struct workspace_part {
    int         x0;
    int         y0;
    int         x1;
    int         y1;
    const char *label;
};

static const struct workspace_part left_half            = {0, 0, 1, 2, "Left half"};
static const struct workspace_part right_half           = {1, 0, 2, 2, "Right half"};
static const struct workspace_part bottom_half          = {0, 1, 2, 2, "Bottom half"};
static const struct workspace_part whole_workspace      = {0, 0, 2, 2, "Maximize"};
static const struct workspace_part top_left_quarter     = {0, 0, 1, 1, "Top left quarter"};
static const struct workspace_part top_right_quarter    = {1, 0, 2, 1, "Top right quarter"};
static const struct workspace_part bottom_left_quarter  = {0, 1, 1, 2, "Bottom left quarter"};
static const struct workspace_part bottom_right_quarter = {1, 1, 2, 2, "Bottom right quarter"};

static const struct workspace_part *const parts_by_edge[SCREEN_EDGE_COUNT][EDGE_STRETCH_COUNT] = {
    [SCREEN_EDGE_LEFT]   = {&top_left_quarter,    &left_half,       &bottom_left_quarter },
    [SCREEN_EDGE_RIGHT]  = {&top_right_quarter,   &right_half,      &bottom_right_quarter},
    [SCREEN_EDGE_TOP]    = {&top_left_quarter,    &whole_workspace, &top_right_quarter   },
    [SCREEN_EDGE_BOTTOM] = {&bottom_left_quarter, &bottom_half,     &bottom_right_quarter},
};

static enum edge_stretch
find_stretch(
    int value,
    int start,
    int length,
    int corner_size
)
{
    if (value < start + corner_size) {
        return EDGE_STRETCH_START;
    }

    if (value >= start + length - corner_size) {
        return EDGE_STRETCH_END;
    }

    return EDGE_STRETCH_MIDDLE;
}

/* SCREEN_EDGE_COUNT when the pointer is near no edge; left and right win over top and bottom. */
static enum screen_edge
find_screen_edge(
    const struct window_snap_scene *scene,
    int                             x,
    int                             y
)
{
    const struct compositor_rectangle *output                        = &scene->output_area;
    const int                          zone                          = scene->edge_zone_size;
    const bool                         near_edges[SCREEN_EDGE_COUNT] = {
        x < output->x + zone,
        x >= output->x + output->width - zone,
        y < output->y + zone,
        y >= output->y + output->height - zone,
    };

    for (int edge = 0; edge < SCREEN_EDGE_COUNT; edge++) {
        if (near_edges[edge]) {
            return (enum screen_edge)edge;
        }
    }

    return SCREEN_EDGE_COUNT;
}

static bool
is_side_edge(
    enum screen_edge edge
)
{
    return edge == SCREEN_EDGE_LEFT || edge == SCREEN_EDGE_RIGHT;
}

/* Side edges stretch from top to bottom, the top and bottom edges from left to right. */
static enum edge_stretch
find_stretch_along_edge(
    const struct window_snap_scene *scene,
    enum screen_edge                edge,
    int                             x,
    int                             y
)
{
    const struct compositor_rectangle *output      = &scene->output_area;
    const int                          corner_size = scene->edge_zone_size * CORNER_ZONE_FACTOR;

    if (is_side_edge(edge)) {
        return find_stretch(y, output->y, output->height, corner_size);
    }

    return find_stretch(x, output->x, output->width, corner_size);
}

static struct compositor_rectangle
workspace_part_rectangle(
    const struct compositor_rectangle *workspace,
    const struct workspace_part       *part
)
{
    int left   = workspace->x + workspace->width * part->x0 / 2;
    int top    = workspace->y + workspace->height * part->y0 / 2;
    int right  = workspace->x + workspace->width * part->x1 / 2;
    int bottom = workspace->y + workspace->height * part->y1 / 2;

    return (struct compositor_rectangle){left, top, right - left, bottom - top};
}

static void
fill_screen_area(
    const struct window_snap_scene *scene,
    const struct workspace_part    *part,
    struct window_snap_placement   *placement
)
{
    placement->action  = WINDOW_SNAP_FILLS_SCREEN_AREA;
    placement->preview = workspace_part_rectangle(&scene->workspace_area, part);
    placement->label   = g_strdup(part->label);
}

/* Returns false when the pointer is near no screen edge. */
static bool
find_screen_area(
    const struct window_snap_scene *scene,
    int                             x,
    int                             y,
    struct window_snap_placement   *placement
)
{
    enum screen_edge edge = find_screen_edge(scene, x, y);

    if (edge == SCREEN_EDGE_COUNT) {
        return false;
    }

    fill_screen_area(scene, parts_by_edge[edge][find_stretch_along_edge(scene, edge, x, y)], placement);

    return true;
}

/* Other tiled windows. */

static bool
can_be_target(
    const struct window_snap_scene *scene,
    const struct compositor_window *window
)
{
    if (window->container_id == scene->dragged_window->container_id) {
        return false;
    }

    if (window->is_floating) {
        return false;
    }

    return window->is_visible;
}

static bool
is_target_under_pointer(
    const struct window_snap_scene *scene,
    const struct compositor_window *window,
    int                             x,
    int                             y
)
{
    struct compositor_rectangle frame = window_snap_frame_of(window);

    if (!can_be_target(scene, window)) {
        return false;
    }

    return rectangle_contains(&frame, x, y);
}

static const struct compositor_window *
find_target_window(
    const struct window_snap_scene *scene,
    int                             x,
    int                             y
)
{
    for (guint i = 0; i < scene->windows->len; i++) {
        const struct compositor_window *window = g_ptr_array_index((GPtrArray *)scene->windows, i);

        if (is_target_under_pointer(scene, window, x, y)) {
            return window;
        }
    }

    return NULL;
}

/* The title bar, or the top of the window for windows without one. */
static bool
is_over_title_bar(
    const struct window_snap_scene *scene,
    const struct compositor_window *target,
    int                             y
)
{
    struct compositor_rectangle frame       = window_snap_frame_of(target);
    int                         band_height = MAX(target->title_bar_height, scene->edge_zone_size);

    return y < frame.y + band_height;
}

/* The side whose edge is nearest, compared in fractions of the window's size. */
static enum window_snap_side
find_nearest_side(
    const struct compositor_rectangle *frame,
    int                                x,
    int                                y
)
{
    double       from_left   = (double)(x - frame->x) / MAX(frame->width, 1);
    double       from_top    = (double)(y - frame->y) / MAX(frame->height, 1);
    const double distances[] = {from_left, 1.0 - from_left, from_top, 1.0 - from_top};
    int          nearest     = 0;

    for (int side = 1; side < 4; side++) {
        nearest = distances[side] < distances[nearest] ? side : nearest;
    }

    return (enum window_snap_side)nearest;
}

static struct compositor_rectangle
half_on_side(
    const struct compositor_rectangle *frame,
    enum window_snap_side              side
)
{
    int half_width  = frame->width / 2;
    int half_height = frame->height / 2;

    const struct compositor_rectangle halves[] = {
        [WINDOW_SNAP_SIDE_LEFT]   = {frame->x,              frame->y,               half_width,                frame->height              },
        [WINDOW_SNAP_SIDE_RIGHT]  = {frame->x + half_width, frame->y,               frame->width - half_width, frame->height              },
        [WINDOW_SNAP_SIDE_TOP]    = {frame->x,              frame->y,               frame->width,              half_height                },
        [WINDOW_SNAP_SIDE_BOTTOM] = {frame->x,              frame->y + half_height, frame->width,              frame->height - half_height},
    };

    return halves[side];
}

static const char *const beside_label_formats[] = {
    [WINDOW_SNAP_SIDE_LEFT]   = "Left of %s",
    [WINDOW_SNAP_SIDE_RIGHT]  = "Right of %s",
    [WINDOW_SNAP_SIDE_TOP]    = "Above %s",
    [WINDOW_SNAP_SIDE_BOTTOM] = "Below %s",
};

static void
place_beside_target(
    const struct compositor_window *target,
    int                             x,
    int                             y,
    struct window_snap_placement   *placement
)
{
    struct compositor_rectangle frame = window_snap_frame_of(target);

    placement->action  = WINDOW_SNAP_TILES_BESIDE;
    placement->side    = find_nearest_side(&frame, x, y);
    placement->preview = half_on_side(&frame, placement->side);
    placement->label   = g_strdup_printf(beside_label_formats[placement->side], target->application_name);
}

static void
place_as_tab_of_target(
    const struct compositor_window *target,
    struct window_snap_placement   *placement
)
{
    placement->action  = WINDOW_SNAP_TABS_WITH;
    placement->preview = window_snap_frame_of(target);
    placement->label   = g_strdup_printf("Tab with %s", target->application_name);
}

/* Over the target's title bar the window becomes its tab; elsewhere over it, it tiles beside it. */
static void
place_relative_to_target(
    const struct window_snap_scene *scene,
    const struct compositor_window *target,
    int                             x,
    int                             y,
    struct window_snap_placement   *placement
)
{
    placement->target = target;

    if (is_over_title_bar(scene, target, y)) {
        place_as_tab_of_target(target, placement);
        return;
    }

    place_beside_target(target, x, y, placement);
}

/* Returns false when the pointer is over no other tiled window. */
static bool
find_tiling_place(
    const struct window_snap_scene *scene,
    int                             x,
    int                             y,
    struct window_snap_placement   *placement
)
{
    const struct compositor_window *target = find_target_window(scene, x, y);

    if (target == NULL) {
        return false;
    }

    place_relative_to_target(scene, target, x, y, placement);

    return true;
}

/* Anywhere else. */

static bool
restores_size(
    const struct window_snap_scene *scene
)
{
    if (scene->restored_width <= 0) {
        return false;
    }

    return scene->restored_height > 0;
}

/*
 * The restored frame keeps the pointer at the same place along its title
 * bar, in proportion, and at the same height in it, as Windows does.
 */
static struct compositor_rectangle
restored_frame_under_pointer(
    const struct window_snap_scene *scene,
    int                             x,
    int                             y
)
{
    struct compositor_rectangle frame    = window_snap_frame_of(scene->dragged_window);
    double                      fraction = (double)(scene->drag_start_x - frame.x) / MAX(frame.width, 1);

    return (struct compositor_rectangle){
        x - (int)(fraction * scene->restored_width),
        y - (scene->drag_start_y - frame.y),
        scene->restored_width,
        scene->restored_height,
    };
}

static struct compositor_rectangle
moved_frame(
    const struct window_snap_scene *scene,
    int                             x,
    int                             y
)
{
    struct compositor_rectangle frame = window_snap_frame_of(scene->dragged_window);

    frame.x += x - scene->drag_start_x;
    frame.y += y - scene->drag_start_y;

    return frame;
}

static bool
can_move_freely(
    const struct window_snap_scene *scene
)
{
    if (scene->dragged_window->is_floating) {
        return true;
    }

    return restores_size(scene);
}

/* A floating or restored window follows the pointer; a tiled one stays where it is. */
static void
find_free_place(
    const struct window_snap_scene *scene,
    int                             x,
    int                             y,
    struct window_snap_placement   *placement
)
{
    if (!can_move_freely(scene)) {
        return;
    }

    placement->action  = WINDOW_SNAP_MOVES_FREELY;
    placement->resizes = restores_size(scene);
    placement->preview = placement->resizes ? restored_frame_under_pointer(scene, x, y) : moved_frame(scene, x, y);
    placement->label   = g_strdup("Move");
}

void
window_snap_placement_find(
    const struct window_snap_scene *scene,
    int                             pointer_x,
    int                             pointer_y,
    struct window_snap_placement   *placement
)
{
    *placement = (struct window_snap_placement){.action = WINDOW_SNAP_DOES_NOTHING};

    if (find_screen_area(scene, pointer_x, pointer_y, placement)) {
        return;
    }

    if (find_tiling_place(scene, pointer_x, pointer_y, placement)) {
        return;
    }

    find_free_place(scene, pointer_x, pointer_y, placement);
}

void
window_snap_placement_clear(
    struct window_snap_placement *placement
)
{
    g_clear_pointer(&placement->label, g_free);
}

/* Commands. */

static char *
build_screen_area_command(
    const struct window_snap_placement     *placement,
    const struct compositor_window         *dragged_window,
    const struct window_snap_border_widths *border_widths
)
{
    const struct compositor_rectangle *area = &placement->preview;

    return g_strdup_printf(
        "[con_id=%" PRId64
        "] floating enable, border normal %d, resize set width %d px height %d px, move absolute position %d %d",
        dragged_window->container_id,
        border_widths->floating_border_width,
        area->width,
        area->height,
        area->x,
        area->y
    );
}

/* A window that also gets back its former size is placed like a screen area. */
static char *
build_free_move_command(
    const struct window_snap_placement     *placement,
    const struct compositor_window         *dragged_window,
    const struct window_snap_border_widths *border_widths
)
{
    if (placement->resizes) {
        return build_screen_area_command(placement, dragged_window, border_widths);
    }

    return g_strdup_printf(
        "[con_id=%" PRId64 "] move absolute position %d %d",
        dragged_window->container_id,
        placement->preview.x,
        placement->preview.y
    );
}

static bool
side_is_horizontal(
    enum window_snap_side side
)
{
    return side == WINDOW_SNAP_SIDE_LEFT || side == WINDOW_SNAP_SIDE_RIGHT;
}

/* The target's container must run in the drop's direction, else the target gets its own. */
static void
append_split_if_needed(
    GString                            *command,
    const struct window_snap_placement *placement
)
{
    bool                             horizontal    = side_is_horizontal(placement->side);
    enum compositor_container_layout wanted_layout = horizontal ? COMPOSITOR_CONTAINER_LAYOUT_SIDE_BY_SIDE
                                                                : COMPOSITOR_CONTAINER_LAYOUT_ON_TOP_OF_EACH_OTHER;

    if (placement->target->parent_layout == wanted_layout) {
        return;
    }

    g_string_append_printf(command, "[con_id=%" PRId64 "] split %s; ", placement->target->container_id, horizontal ? "h" : "v");
}

static bool
is_tab_layout(
    enum compositor_container_layout layout
)
{
    return layout == COMPOSITOR_CONTAINER_LAYOUT_TABBED || layout == COMPOSITOR_CONTAINER_LAYOUT_STACKED;
}

/* Tabbed or stacked containers take the window as a tab directly; anything else is wrapped first. */
static void
append_tab_container_if_needed(
    GString                            *command,
    const struct window_snap_placement *placement
)
{
    if (is_tab_layout(placement->target->parent_layout)) {
        return;
    }

    g_string_append_printf(command, "[con_id=%" PRId64 "] split v, layout tabbed; ", placement->target->container_id);
}

/* Moving to a mark puts the window after the target: right of it or below it. */
static const char *
direction_before_target(
    const struct window_snap_placement *placement
)
{
    if (placement->action != WINDOW_SNAP_TILES_BESIDE) {
        return NULL;
    }

    if (placement->side == WINDOW_SNAP_SIDE_LEFT) {
        return "left";
    }

    return placement->side == WINDOW_SNAP_SIDE_TOP ? "up" : NULL;
}

static void
append_dragged_window_move(
    GString                                *command,
    const struct window_snap_placement     *placement,
    const struct compositor_window         *dragged_window,
    const struct window_snap_border_widths *border_widths
)
{
    const char *direction = direction_before_target(placement);

    g_string_append_printf(command, "[con_id=%" PRId64 "] ", dragged_window->container_id);
    g_string_append(command, dragged_window->is_floating ? "floating disable, " : "");
    g_string_append_printf(
        command,
        "border normal %d, move container to mark %s",
        border_widths->tiled_border_width,
        drop_mark
    );

    if (direction != NULL) {
        g_string_append_printf(command, ", move %s", direction);
    }
}

/* The same steps as the compositor's own tiling drop: mark the target, move the window to the mark. */
static void
append_move_to_target(
    GString                                *command,
    const struct window_snap_placement     *placement,
    const struct compositor_window         *dragged_window,
    const struct window_snap_border_widths *border_widths
)
{
    int64_t target_id = placement->target->container_id;

    g_string_append_printf(command, "[con_id=%" PRId64 "] mark --add %s; ", target_id, drop_mark);
    append_dragged_window_move(command, placement, dragged_window, border_widths);
    g_string_append_printf(command, "; [con_id=%" PRId64 "] unmark %s", target_id, drop_mark);
}

static char *
build_tiling_command(
    const struct window_snap_placement     *placement,
    const struct compositor_window         *dragged_window,
    const struct window_snap_border_widths *border_widths
)
{
    GString *command = g_string_new(NULL);

    if (placement->action == WINDOW_SNAP_TILES_BESIDE) {
        append_split_if_needed(command, placement);
    } else {
        append_tab_container_if_needed(command, placement);
    }

    append_move_to_target(command, placement, dragged_window, border_widths);

    return g_string_free(command, FALSE);
}

char *
window_snap_placement_build_command(
    const struct window_snap_placement     *placement,
    const struct compositor_window         *dragged_window,
    const struct window_snap_border_widths *border_widths
)
{
    switch (placement->action) {
    case WINDOW_SNAP_FILLS_SCREEN_AREA:
        return build_screen_area_command(placement, dragged_window, border_widths);
    case WINDOW_SNAP_MOVES_FREELY:
        return build_free_move_command(placement, dragged_window, border_widths);
    case WINDOW_SNAP_TILES_BESIDE:
    case WINDOW_SNAP_TABS_WITH:
        return build_tiling_command(placement, dragged_window, border_widths);
    default:
        return NULL;
    }
}
