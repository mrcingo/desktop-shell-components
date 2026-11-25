#include <inttypes.h>
#include <math.h>
#include <string.h>

#include <gtk/gtk.h>

#include "compositor-matched-appearance.h"
#include "compositor-state-tracker.h"
#include "monitor-connector-lookup.h"
#include "window-snap-application.h"
#include "window-snap-keyboard-steps.h"
#include "window-snap-overlay.h"
#include "window-snap-placement.h"
#include "window-snap-settings.h"

static const char command_prefix[] = "nop desktop-shell-window-snap ";
static const char target_mark[]    = "_window-snap-target";

/* What a snapped window goes back to when it is restored. */
struct snap_record {
    enum window_snap_state      state;
    bool                        was_floating;
    struct compositor_rectangle frame;        // its frame before it was snapped
    int64_t                     neighbour_id; // a tiled window to return next to; 0 for none
    enum window_snap_side       neighbour_side;
};

/* One press on a title bar, from the press until the release. */
struct drag_session {
    struct compositor_window    *dragged_window; // copies: the tree may change meanwhile
    GPtrArray                   *windows;        // struct compositor_window on the same workspace
    struct compositor_rectangle  output_area;
    struct compositor_rectangle  workspace_area;
    struct window_snap_overlay  *overlay;
    bool                         holds_cursor_press; // the overlay was given a press, to be released at the end
    bool                         has_start;
    bool                         is_dragging;
    double                       start_x; // layout pixels
    double                       start_y;
    struct window_snap_scene     scene;
    struct window_snap_placement placement;
};

struct window_snap_application {
    const struct window_snap_options *options;
    GtkApplication                   *gtk_application;
    bool                              started_successfully;

    struct compositor_state_tracker      *compositor_state;
    struct window_snap_settings          *settings;
    struct compositor_matched_appearance *appearance;
    GHashTable                           *snap_records; // int64_t container ID -> struct snap_record

    bool                 press_pending;  // pressed; waiting for the tree that shows the mark
    bool                 released_early; // released before that tree came: a click
    struct drag_session *session;        // NULL while nothing is pressed
    int64_t              last_click_window;
    gint64               last_click_time;
};

static struct compositor_ipc_client *
ipc_client_of(
    struct window_snap_application *snap
)
{
    return compositor_state_tracker_get_ipc_client(snap->compositor_state);
}

static void
run_command(
    struct window_snap_application *snap,
    const char                     *command
)
{
    compositor_ipc_client_send_request(ipc_client_of(snap), COMPOSITOR_IPC_REQUEST_RUN_COMMAND, command, NULL, NULL);
}

/* Every placement ends with the placed window focused, as after a drag on Windows. */
static void
run_command_then_focus(
    struct window_snap_application *snap,
    const char                     *command,
    int64_t                         container_id
)
{
    g_autofree char *focusing_command = g_strdup_printf("%s; [con_id=%" PRId64 "] focus", command, container_id);

    run_command(snap, focusing_command);
}

static struct window_snap_border_widths
border_widths_of(
    const struct window_snap_application *snap
)
{
    return (struct window_snap_border_widths){
        .floating_border_width = snap->settings->floating_border_width,
        .tiled_border_width    = snap->settings->tiled_border_width,
    };
}

/* The output showing the window, or NULL when its workspace is hidden. */
static const struct compositor_output *
output_showing(
    const struct compositor_window_tree *tree,
    const struct compositor_window      *window
)
{
    return compositor_window_tree_find_output_showing_window(tree, window->container_id);
}

/* Snap records. */

static struct snap_record *
find_record(
    struct window_snap_application *snap,
    int64_t                         container_id
)
{
    return g_hash_table_lookup(snap->snap_records, &container_id);
}

static struct snap_record *
ensure_record(
    struct window_snap_application *snap,
    int64_t                         container_id
)
{
    struct snap_record *record = find_record(snap, container_id);

    if (record != NULL) {
        return record;
    }

    record = g_new0(struct snap_record, 1);

    g_hash_table_insert(snap->snap_records, g_memdup2(&container_id, sizeof(container_id)), record);

    return record;
}

static void
forget_record(
    struct window_snap_application *snap,
    int64_t                         container_id
)
{
    g_hash_table_remove(snap->snap_records, &container_id);
}

static enum window_snap_state
current_state(
    struct window_snap_application *snap,
    int64_t                         container_id
)
{
    struct snap_record *record = find_record(snap, container_id);

    return record != NULL ? record->state : WINDOW_SNAP_STATE_FREE;
}

static const struct compositor_window *
window_at(
    const GPtrArray *windows,
    guint            index
)
{
    return g_ptr_array_index((GPtrArray *)windows, index);
}

/* The last tiled window before the given one in layout order, or NULL. */
static const struct compositor_window *
find_tiled_window_before(
    const GPtrArray *windows,
    int64_t          container_id
)
{
    const struct compositor_window *previous = NULL;

    for (guint i = 0; i < windows->len && window_at(windows, i)->container_id != container_id; i++) {
        previous = window_at(windows, i)->is_floating ? previous : window_at(windows, i);
    }

    return previous;
}

/* A tiled window returns next to the tiled window before it in layout order, on its right. */
static void
remember_place_before_snapping(
    struct snap_record             *record,
    const struct compositor_window *window,
    const GPtrArray                *windows
)
{
    const struct compositor_window *previous = window->is_floating ? NULL
                                                                   : find_tiled_window_before(windows, window->container_id);

    record->was_floating   = window->is_floating;
    record->frame          = window_snap_frame_of(window);
    record->neighbour_id   = previous != NULL ? previous->container_id : 0;
    record->neighbour_side = WINDOW_SNAP_SIDE_RIGHT;
}

/* Records of closed windows go. */
static void
forget_closed_windows(
    struct window_snap_application      *snap,
    const struct compositor_window_tree *tree
)
{
    GHashTableIter iterator;
    int64_t       *container_id;

    g_hash_table_iter_init(&iterator, snap->snap_records);

    while (g_hash_table_iter_next(&iterator, (gpointer *)&container_id, NULL)) {
        if (compositor_window_tree_find_window(tree, *container_id) == NULL) {
            g_hash_table_iter_remove(&iterator);
        }
    }
}

/* Snapping without dragging: keyboard and double click. */

static char *
build_area_command(
    struct window_snap_application    *snap,
    const struct compositor_window    *window,
    const struct compositor_rectangle *area
)
{
    struct window_snap_placement     placement     = {.action = WINDOW_SNAP_FILLS_SCREEN_AREA, .preview = *area};
    struct window_snap_border_widths border_widths = border_widths_of(snap);

    return window_snap_placement_build_command(&placement, window, &border_widths);
}

/* Back to the tiled place it had, else into the tiling wherever the compositor puts it. */
static char *
build_return_to_tiling_command(
    struct window_snap_application      *snap,
    const struct compositor_window_tree *tree,
    const struct compositor_window      *window,
    const struct snap_record            *record
)
{
    const struct compositor_window  *neighbour     = compositor_window_tree_find_window(tree, record->neighbour_id);
    struct window_snap_border_widths border_widths = border_widths_of(snap);
    struct window_snap_placement     placement     = {
        .action = WINDOW_SNAP_TILES_BESIDE,
        .side   = record->neighbour_side,
        .target = neighbour
    };

    if (neighbour == NULL) {
        return g_strdup_printf(
            "[con_id=%" PRId64 "] floating disable, border normal %d",
            window->container_id,
            border_widths.tiled_border_width
        );
    }

    return window_snap_placement_build_command(&placement, window, &border_widths);
}

static char *
build_restore_command(
    struct window_snap_application      *snap,
    const struct compositor_window_tree *tree,
    const struct compositor_window      *window
)
{
    const struct snap_record *record = ensure_record(snap, window->container_id);

    if (record->was_floating) {
        return build_area_command(snap, window, &record->frame);
    }

    return build_return_to_tiling_command(snap, tree, window, record);
}

static char *
build_snapped_area_command(
    struct window_snap_application      *snap,
    const struct compositor_window_tree *tree,
    const struct compositor_window      *window,
    enum window_snap_state               new_state
)
{
    const struct compositor_output *output = output_showing(tree, window);
    struct compositor_rectangle     area;

    if (output == NULL) {
        return NULL;
    }

    if (!window_snap_state_area(new_state, &output->workspace_area, &area)) {
        return NULL;
    }

    return build_area_command(snap, window, &area);
}

static char *
build_state_command(
    struct window_snap_application      *snap,
    const struct compositor_window_tree *tree,
    const struct compositor_window      *window,
    enum window_snap_state               new_state
)
{
    if (new_state == WINDOW_SNAP_STATE_FREE) {
        return build_restore_command(snap, tree, window);
    }

    return build_snapped_area_command(snap, tree, window, new_state);
}

/* Leaving the free state remembers where the window was, for restoring it later. */
static void
remember_place_when_leaving_free_state(
    struct window_snap_application      *snap,
    const struct compositor_window_tree *tree,
    const struct compositor_window      *window,
    enum window_snap_state               new_state
)
{
    struct snap_record *record = ensure_record(snap, window->container_id);

    if (record->state != WINDOW_SNAP_STATE_FREE) {
        return;
    }

    if (new_state != WINDOW_SNAP_STATE_FREE) {
        remember_place_before_snapping(record, window, output_showing(tree, window)->windows_on_visible_workspace);
    }
}

/* The window must be on a visible workspace. */
static void
apply_state(
    struct window_snap_application      *snap,
    const struct compositor_window_tree *tree,
    const struct compositor_window      *window,
    enum window_snap_state               new_state
)
{
    remember_place_when_leaving_free_state(snap, tree, window, new_state);

    g_autofree char *command = build_state_command(snap, tree, window, new_state);

    if (command == NULL) {
        return;
    }

    run_command_then_focus(snap, command, window->container_id);
    ensure_record(snap, window->container_id)->state = new_state;
}

static const struct compositor_window *
find_focused_window(
    const struct compositor_window_tree *tree
)
{
    for (guint i = 0; i < tree->all_windows->len; i++) {
        const struct compositor_window *window = g_ptr_array_index(tree->all_windows, i);

        if (window->has_keyboard_focus) {
            return window;
        }
    }

    return NULL;
}

/* The window, if it is on a visible workspace; NULL otherwise. */
static const struct compositor_window *
keep_if_shown(
    const struct compositor_window_tree *tree,
    const struct compositor_window      *window
)
{
    if (window == NULL) {
        return NULL;
    }

    return output_showing(tree, window) != NULL ? window : NULL;
}

/* A key steps from the window's state, as on Windows; a state word jumps to it. */
static bool
choose_state_for_word(
    struct window_snap_application *snap,
    const struct compositor_window *window,
    const char                     *word,
    enum window_snap_state         *new_state
)
{
    enum window_snap_key key;

    if (window_snap_parse_state(word, new_state)) {
        return true;
    }

    if (!window_snap_parse_key(word, &key)) {
        return false;
    }

    *new_state = window_snap_state_after_key(current_state(snap, window->container_id), key);

    return true;
}

/* The focused window if it is on a visible workspace, else NULL; tree may be NULL. */
static const struct compositor_window *
find_focused_shown_window(
    const struct compositor_window_tree *tree
)
{
    if (tree == NULL) {
        return NULL;
    }

    return keep_if_shown(tree, find_focused_window(tree));
}

static void
snap_focused_window(
    struct window_snap_application *snap,
    const char                     *word
)
{
    const struct compositor_window_tree *tree   = compositor_state_tracker_get_window_tree(snap->compositor_state);
    const struct compositor_window      *window = find_focused_shown_window(tree);
    enum window_snap_state               new_state;

    if (window == NULL) {
        return;
    }

    if (choose_state_for_word(snap, window, word, &new_state)) {
        apply_state(snap, tree, window, new_state);
    }
}

static void
toggle_maximized(
    struct window_snap_application *snap,
    int64_t                         container_id
)
{
    const struct compositor_window_tree *tree   = compositor_state_tracker_get_window_tree(snap->compositor_state);
    const struct compositor_window      *window = keep_if_shown(tree, compositor_window_tree_find_window(tree, container_id));
    bool                                 is_maximized = current_state(snap, container_id) == WINDOW_SNAP_STATE_MAXIMIZED;

    if (window == NULL) {
        return;
    }

    apply_state(snap, tree, window, is_maximized ? WINDOW_SNAP_STATE_FREE : WINDOW_SNAP_STATE_MAXIMIZED);
}

static bool
is_double_click(
    struct window_snap_application *snap,
    int64_t                         container_id,
    gint64                          now
)
{
    int double_click_milliseconds = 400;

    g_object_get(gtk_settings_get_default(), "gtk-double-click-time", &double_click_milliseconds, NULL);

    if (container_id != snap->last_click_window) {
        return false;
    }

    return now - snap->last_click_time <= (gint64)double_click_milliseconds * 1000;
}

static void
focus_window(
    struct window_snap_application *snap,
    int64_t                         container_id
)
{
    g_autofree char *command = g_strdup_printf("[con_id=%" PRId64 "] focus", container_id);

    run_command(snap, command);
}

static void
remember_click(
    struct window_snap_application *snap,
    int64_t                         container_id,
    gint64                          now
)
{
    snap->last_click_window = container_id;
    snap->last_click_time   = now;
}

/* A click focuses the window, also switching to its tab, as the compositor's own title bars do. */
static void
handle_click(
    struct window_snap_application *snap,
    int64_t                         container_id
)
{
    gint64 now = g_get_monotonic_time();

    focus_window(snap, container_id);

    if (!is_double_click(snap, container_id, now)) {
        remember_click(snap, container_id, now);
        return;
    }

    remember_click(snap, 0, now);
    toggle_maximized(snap, container_id);
}

/* Dragging. */

static void
destroy_session(
    struct drag_session *session
)
{
    if (session == NULL) {
        return;
    }

    window_snap_overlay_destroy(session->overlay);
    window_snap_placement_clear(&session->placement);
    g_ptr_array_unref(session->windows);
    compositor_window_free(session->dragged_window);
    g_free(session);
}

/* The press given to the overlay is always released, so the compositor's count of held buttons stays right. */
static void
release_cursor_press(
    struct window_snap_application *snap,
    const struct drag_session      *session
)
{
    if (!session->holds_cursor_press) {
        return;
    }

    run_command(snap, "seat - cursor release button1");
}

/*
 * The overlay goes first, so that focus and placement commands sent after
 * it are not undone by the compositor giving the keyboard back. Returns the
 * session, which the caller destroys.
 */
static struct drag_session *
close_session(
    struct window_snap_application *snap
)
{
    struct drag_session *session = g_steal_pointer(&snap->session);

    g_clear_pointer(&session->overlay, window_snap_overlay_destroy);
    release_cursor_press(snap, session);

    return session;
}

static void
end_session(
    struct window_snap_application *snap
)
{
    if (snap->session == NULL) {
        return;
    }

    destroy_session(close_session(snap));
}

/* A snapped window dragged away gets back the size it had before, as on Windows. */
static void
restore_size_when_snapped(
    struct window_snap_application *snap,
    struct drag_session            *session
)
{
    struct snap_record *record = find_record(snap, session->dragged_window->container_id);

    if (record == NULL) {
        return;
    }

    if (record->state == WINDOW_SNAP_STATE_FREE) {
        return;
    }

    session->scene.restored_width  = record->frame.width;
    session->scene.restored_height = record->frame.height;
}

static void
prepare_scene(
    struct window_snap_application *snap,
    struct drag_session            *session
)
{
    session->scene = (struct window_snap_scene){
        .output_area    = session->output_area,
        .workspace_area = session->workspace_area,
        .dragged_window = session->dragged_window,
        .windows        = session->windows,
        .edge_zone_size = snap->settings->edge_zone_size,
        .drag_start_x   = (int)session->start_x,
        .drag_start_y   = (int)session->start_y,
    };

    restore_size_when_snapped(snap, session);
}

static GdkRectangle
to_output_pixels(
    const struct drag_session         *session,
    const struct compositor_rectangle *rectangle
)
{
    return (GdkRectangle){
        rectangle->x - session->output_area.x,
        rectangle->y - session->output_area.y,
        rectangle->width,
        rectangle->height,
    };
}

static void
show_placement(
    struct window_snap_application *snap,
    struct drag_session            *session
)
{
    GdkRectangle preview = to_output_pixels(session, &session->placement.preview);
    const char  *label   = snap->settings->shows_labels ? session->placement.label : NULL;

    if (session->placement.action == WINDOW_SNAP_DOES_NOTHING) {
        window_snap_overlay_show_preview(session->overlay, NULL, NULL, 0);
        return;
    }

    window_snap_overlay_show_preview(session->overlay, &preview, label, snap->settings->preview_opacity);
}

static void
update_placement(
    struct window_snap_application *snap,
    struct drag_session            *session,
    double                          x,
    double                          y
)
{
    window_snap_placement_clear(&session->placement);
    window_snap_placement_find(&session->scene, (int)x, (int)y, &session->placement);
    show_placement(snap, session);
}

/* Moving less than the threshold is still a click, as everywhere in GTK. */
static void
start_dragging_beyond_threshold(
    struct window_snap_application *snap,
    struct drag_session            *session,
    double                          x,
    double                          y
)
{
    if (hypot(x - session->start_x, y - session->start_y) < snap->settings->drag_threshold) {
        return;
    }

    session->is_dragging = true;

    prepare_scene(snap, session);
    window_snap_overlay_take_keyboard(session->overlay);
}

static void
follow_pointer(
    struct window_snap_application *snap,
    struct drag_session            *session,
    double                          x,
    double                          y
)
{
    if (!session->is_dragging) {
        start_dragging_beyond_threshold(snap, session, x, y);
    }

    if (session->is_dragging) {
        update_placement(snap, session, x, y);
    }
}

/* The first position the overlay learns is where the press was. */
static void
remember_start(
    struct drag_session *session,
    double               x,
    double               y
)
{
    session->has_start = true;
    session->start_x   = x;
    session->start_y   = y;
}

/* Positions arrive in the monitor's pixels; placements work in layout pixels. */
static void
handle_pointer_moved(
    void  *user_data,
    double x,
    double y
)
{
    struct window_snap_application *snap     = user_data;
    double                          layout_x = x + snap->session->output_area.x;
    double                          layout_y = y + snap->session->output_area.y;

    if (!snap->session->has_start) {
        remember_start(snap->session, layout_x, layout_y);
        return;
    }

    follow_pointer(snap, snap->session, layout_x, layout_y);
}

static bool
area_matches_state(
    const struct drag_session         *session,
    const struct compositor_rectangle *area,
    enum window_snap_state             state
)
{
    struct compositor_rectangle state_area;

    if (!window_snap_state_area(state, &session->workspace_area, &state_area)) {
        return false;
    }

    return memcmp(&state_area, area, sizeof(state_area)) == 0;
}

/* The snapped state a dropped screen area matches, so the keyboard and a later drag continue from it. */
static enum window_snap_state
state_of_area(
    const struct drag_session         *session,
    const struct compositor_rectangle *area
)
{
    for (int state = WINDOW_SNAP_STATE_LEFT_HALF; state < WINDOW_SNAP_STATE_COUNT; state++) {
        if (area_matches_state(session, area, (enum window_snap_state)state)) {
            return (enum window_snap_state)state;
        }
    }

    return WINDOW_SNAP_STATE_MAXIMIZED;
}

static void
remember_snapped_area(
    struct window_snap_application *snap,
    struct drag_session            *session
)
{
    struct snap_record *record = ensure_record(snap, session->dragged_window->container_id);

    if (record->state == WINDOW_SNAP_STATE_FREE) {
        remember_place_before_snapping(record, session->dragged_window, session->windows);
    }

    record->state = state_of_area(session, &session->placement.preview);
}

/* Windows put into a screen area can be restored; anywhere else they are free again. */
static void
record_drop(
    struct window_snap_application *snap,
    struct drag_session            *session
)
{
    if (session->placement.action != WINDOW_SNAP_FILLS_SCREEN_AREA) {
        forget_record(snap, session->dragged_window->container_id);
        return;
    }

    remember_snapped_area(snap, session);
}

static void
drop_window(
    struct window_snap_application *snap,
    struct drag_session            *session
)
{
    struct window_snap_border_widths border_widths = border_widths_of(snap);

    g_autofree char *command = window_snap_placement_build_command(
        &session->placement,
        session->dragged_window,
        &border_widths
    );

    if (command == NULL) {
        return;
    }

    record_drop(snap, session);
    run_command_then_focus(snap, command, session->dragged_window->container_id);
}

/* The final position decides the placement; the overlay is already gone. */
static void
finish_press(
    struct window_snap_application *snap,
    struct drag_session            *session,
    double                          x,
    double                          y
)
{
    if (!session->is_dragging) {
        handle_click(snap, session->dragged_window->container_id);
        return;
    }

    window_snap_placement_clear(&session->placement);
    window_snap_placement_find(
        &session->scene,
        (int)(x + session->output_area.x),
        (int)(y + session->output_area.y),
        &session->placement
    );
    drop_window(snap, session);
}

static void
handle_button_released(
    void  *user_data,
    double x,
    double y
)
{
    struct drag_session *session = close_session(user_data);

    finish_press(user_data, session, x, y);
    destroy_session(session);
}

/* As on Windows, pressing a title bar focuses its window even when the drag is cancelled. */
static void
handle_cancelled(
    void *user_data
)
{
    struct drag_session *session = close_session(user_data);

    focus_window(user_data, session->dragged_window->container_id);
    destroy_session(session);
}

/*
 * The compositor gave the title bar's press to the binding, so no surface
 * holds it and the release would reach nobody. Pressing again, through the
 * compositor's own cursor command, gives the press to the overlay: it then
 * follows the pointer and gets the user's release.
 */
static void
handle_shown(
    void *user_data
)
{
    struct window_snap_application *snap = user_data;

    snap->session->holds_cursor_press = true;

    run_command(snap, "seat - cursor move 1 0; seat - cursor move -1 0; seat - cursor press button1");
}

static const struct window_snap_overlay_handler overlay_handler = {
    .handle_shown           = handle_shown,
    .handle_pointer_moved   = handle_pointer_moved,
    .handle_button_released = handle_button_released,
    .handle_cancelled       = handle_cancelled,
};

static GPtrArray *
copy_windows(
    const GPtrArray *windows
)
{
    GPtrArray *copies = g_ptr_array_new_with_free_func((GDestroyNotify)compositor_window_free);

    for (guint i = 0; i < windows->len; i++) {
        g_ptr_array_add(copies, compositor_window_copy(window_at(windows, i)));
    }

    return copies;
}

static struct window_snap_overlay *
create_overlay_on(
    struct window_snap_application *snap,
    const struct compositor_output *output
)
{
    GdkMonitor *monitor = monitor_connector_lookup_find(gdk_display_get_default(), output->output_name);

    return window_snap_overlay_create(snap->gtk_application, monitor, &overlay_handler, snap);
}

static struct drag_session *
create_session(
    struct window_snap_application *snap,
    const struct compositor_output *output,
    const struct compositor_window *window
)
{
    struct drag_session *session = g_new0(struct drag_session, 1);

    session->dragged_window = compositor_window_copy(window);
    session->windows        = copy_windows(output->windows_on_visible_workspace);
    session->output_area    = output->output_area;
    session->workspace_area = output->workspace_area;
    session->overlay        = create_overlay_on(snap, output);

    return session;
}

/* A release that came before the tree, or a window on no visible output, makes the press a click. */
static bool
press_is_click(
    const struct window_snap_application *snap,
    const struct compositor_output       *output
)
{
    if (snap->released_early) {
        return true;
    }

    return output == NULL;
}

static void
start_press_on_window(
    struct window_snap_application      *snap,
    const struct compositor_window_tree *tree,
    const struct compositor_window      *window
)
{
    const struct compositor_output *output = output_showing(tree, window);

    if (press_is_click(snap, output)) {
        handle_click(snap, window->container_id);
        return;
    }

    snap->session = create_session(snap, output, window);
}

/* The mark names the pressed window; it goes once read. */
static void
begin_press(
    struct window_snap_application      *snap,
    const struct compositor_window_tree *tree
)
{
    const struct compositor_window *window = compositor_window_tree_find_marked_window(tree, target_mark);

    g_autofree char *unmark = g_strdup_printf("unmark %s", target_mark);

    if (window == NULL) {
        return;
    }

    run_command(snap, unmark);
    start_press_on_window(snap, tree, window);
}

/* Compositor bindings. */

static void
handle_window_tree_updated(
    void                                *user_data,
    const struct compositor_window_tree *tree
)
{
    struct window_snap_application *snap = user_data;

    forget_closed_windows(snap, tree);

    if (!snap->press_pending) {
        return;
    }

    snap->press_pending = false;

    begin_press(snap, tree);
}

/* The press binding also marks the window; the next tree shows which one. */
static void
handle_press(
    struct window_snap_application *snap
)
{
    end_session(snap);

    snap->press_pending  = true;
    snap->released_early = false;

    compositor_state_tracker_refresh_window_tree(snap->compositor_state);
}

static bool
has_undragged_press(
    const struct window_snap_application *snap
)
{
    if (snap->session == NULL) {
        return false;
    }

    return !snap->session->is_dragging;
}

/*
 * The release binding only fires while the pointer is still over the title
 * bar, that is before the overlay covers it: the press was a quick click.
 */
static void
handle_release(
    struct window_snap_application *snap
)
{
    if (snap->press_pending) {
        snap->released_early = true;
        return;
    }

    if (has_undragged_press(snap)) {
        handle_button_released(snap, 0, 0);
    }
}

static void
handle_word(
    struct window_snap_application *snap,
    const char                     *word
)
{
    if (g_strcmp0(word, "press") == 0) {
        handle_press(snap);
        return;
    }

    if (g_strcmp0(word, "release") == 0) {
        handle_release(snap);
        return;
    }

    snap_focused_window(snap, word);
}

/* The word after "nop desktop-shell-window-snap" in a binding's command list, or NULL. */
static char *
find_command_word(
    const char *binding_command
)
{
    g_auto(GStrv) parts = g_strsplit_set(binding_command, ",;", -1);

    for (char **part = parts; *part != NULL; part++) {
        const char *trimmed = g_strstrip(*part);

        if (g_str_has_prefix(trimmed, command_prefix)) {
            return g_strdup(trimmed + strlen(command_prefix));
        }
    }

    return NULL;
}

static void
handle_key_binding_command(
    void       *user_data,
    const char *command
)
{
    struct window_snap_application *snap = user_data;

    g_autofree char *word = find_command_word(command);

    if (word == NULL) {
        return;
    }

    if (!snap->settings->is_enabled) {
        return;
    }

    handle_word(snap, word);
}

static void
handle_compositor_gone(
    void *user_data
)
{
    struct window_snap_application *snap = user_data;

    g_application_quit(G_APPLICATION(snap->gtk_application));
}

static void
reload_settings(
    struct window_snap_application *snap
)
{
    window_snap_settings_destroy(snap->settings);

    snap->settings = window_snap_settings_load(snap->options->configuration_override_path);

    compositor_matched_appearance_refresh(snap->appearance, snap->settings->follows_sway_bar);
}

static void
handle_compositor_configuration_reloaded(
    void *user_data
)
{
    struct window_snap_application *snap = user_data;

    replaced_executable_restart_if_replaced(snap->options->executable_restart);
    reload_settings(snap);
}

static const struct compositor_state_tracker_handler compositor_state_handler = {
    .handle_window_tree_updated               = handle_window_tree_updated,
    .handle_key_binding_command               = handle_key_binding_command,
    .handle_compositor_gone                   = handle_compositor_gone,
    .handle_compositor_configuration_reloaded = handle_compositor_configuration_reloaded,
};

/* Lifetime. */

static void
report_connection_failure(
    GError *error
)
{
    g_printerr("Cannot connect to the compositor: %s\n", error->message);
    g_error_free(error);
}

static bool
connect_to_compositor(
    struct window_snap_application *snap
)
{
    GError *error = NULL;

    snap->compositor_state = compositor_state_tracker_connect(
        snap->options->compositor_socket_path,
        &compositor_state_handler,
        snap,
        &error
    );

    if (snap->compositor_state == NULL) {
        report_connection_failure(error);
        return false;
    }

    return true;
}

static void
create_appearance(
    struct window_snap_application *snap
)
{
    snap->appearance = compositor_matched_appearance_create(
        gdk_display_get_default(),
        ipc_client_of(snap),
        snap->options->data_directory,
        "window-snap",
        snap->options->style_sheet_override_path
    );
}

/* Widgets can only be created once GtkApplication has started GTK. */
static void
handle_activate(
    GApplication *gtk_application,
    void         *user_data
)
{
    struct window_snap_application *snap = user_data;

    if (!connect_to_compositor(snap)) {
        return;
    }

    snap->started_successfully = true;

    create_appearance(snap);
    reload_settings(snap);

    /* No window stays open between presses; keep running anyway. */
    g_application_hold(gtk_application);
}

/* The overlay must go while GTK still runs; the appearance goes before the connection it uses. */
static void
handle_shutdown(
    GApplication *gtk_application,
    void         *user_data
)
{
    struct window_snap_application *snap = user_data;

    end_session(snap);
    g_clear_pointer(&snap->appearance, compositor_matched_appearance_destroy);
    g_clear_pointer(&snap->compositor_state, compositor_state_tracker_destroy);
    g_clear_pointer(&snap->settings, window_snap_settings_destroy);
}

bool
window_snap_application_run(
    const struct window_snap_options *options
)
{
    struct window_snap_application snap = {
        .options         = options,
        .gtk_application = gtk_application_new(NULL, G_APPLICATION_NON_UNIQUE),
        .snap_records    = g_hash_table_new_full(g_int64_hash, g_int64_equal, g_free, g_free),
    };

    g_signal_connect(snap.gtk_application, "activate", G_CALLBACK(handle_activate), &snap);
    g_signal_connect(snap.gtk_application, "shutdown", G_CALLBACK(handle_shutdown), &snap);
    g_application_run(G_APPLICATION(snap.gtk_application), 0, NULL);
    g_object_unref(snap.gtk_application);
    g_hash_table_unref(snap.snap_records);

    return snap.started_successfully;
}
