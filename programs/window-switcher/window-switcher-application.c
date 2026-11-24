#include <string.h>

#include <gtk/gtk.h>

#include "compositor-matched-appearance.h"
#include "compositor-state-tracker.h"
#include "monitor-connector-lookup.h"
#include "toplevel-window-capture.h"
#include "window-label-format.h"
#include "window-switcher-application.h"
#include "window-switcher-overlay.h"
#include "window-switcher-settings.h"

static const char select_next_command[]     = "nop desktop-shell-window-switcher next";
static const char select_previous_command[] = "nop desktop-shell-window-switcher previous";

struct window_switcher_application {
    const struct window_switcher_options *options;
    GtkApplication                       *gtk_application;
    bool                                  started_successfully;

    struct compositor_state_tracker      *compositor_state;
    struct window_switcher_settings      *settings;
    struct compositor_matched_appearance *appearance;
    struct toplevel_window_capture       *window_capture; // NULL without Wayland
    struct window_switcher_overlay       *overlay;
    GPtrArray                            *pending_previews; // struct pending_preview
};

struct pending_preview {
    struct window_switcher_application      *switcher;
    int64_t                                  container_id;
    struct toplevel_window_snapshot_request *snapshot_request;
};

static void
destroy_pending_preview(
    void *data
)
{
    struct pending_preview *preview = data;

    toplevel_window_capture_cancel_snapshots(preview->snapshot_request);
    g_free(preview);
}

/* A NULL snapshot means the capture has ended and freed its request. */
static void
handle_window_snapshot(
    GdkTexture *snapshot,
    void       *user_data
)
{
    struct pending_preview *preview = user_data;

    if (snapshot != NULL) {
        window_switcher_overlay_set_preview(preview->switcher->overlay, preview->container_id, snapshot);
        return;
    }

    preview->snapshot_request = NULL;

    g_ptr_array_remove_fast(preview->switcher->pending_previews, preview);
}

/* Windows are captured no larger than their preview is drawn. */
static struct toplevel_window_snapshot_size_limit
preview_size_limit(
    struct window_switcher_application *switcher
)
{
    struct toplevel_window_snapshot_size_limit size_limit = {0};

    window_switcher_overlay_get_preview_pixel_size(switcher->overlay, &size_limit.maximum_width, &size_limit.maximum_height);

    return size_limit;
}

static struct pending_preview *
create_pending_preview(
    struct window_switcher_application *switcher,
    const struct compositor_window     *window
)
{
    struct pending_preview *preview = g_new0(struct pending_preview, 1);

    preview->switcher         = switcher;
    preview->container_id     = window->container_id;
    preview->snapshot_request = toplevel_window_capture_request_snapshots(
        switcher->window_capture,
        window->toplevel_capture_identifier,
        preview_size_limit(switcher),
        handle_window_snapshot,
        preview
    );

    return preview;
}

/* A window that cannot be captured keeps an empty preview. */
static void
request_preview(
    struct window_switcher_application *switcher,
    const struct compositor_window     *window
)
{
    struct pending_preview *preview = create_pending_preview(switcher, window);

    if (preview->snapshot_request == NULL) {
        g_free(preview);
        return;
    }

    g_ptr_array_add(switcher->pending_previews, preview);
}

static void
request_previews(
    struct window_switcher_application *switcher,
    GPtrArray                          *windows
)
{
    if (!switcher->settings->shows_previews) {
        return;
    }

    for (guint i = 0; i < windows->len; i++) {
        request_preview(switcher, g_ptr_array_index(windows, i));
    }
}

static int
compare_focus_recency(
    const void *first,
    const void *second,
    void       *user_data
)
{
    const struct compositor_window *first_window  = *(const struct compositor_window *const *)first;
    const struct compositor_window *second_window = *(const struct compositor_window *const *)second;

    return window_focus_history_compare_recency(user_data, first_window->container_id, second_window->container_id);
}

/* The focused workspace's windows, or every window, as the settings ask. */
static GPtrArray *
candidate_windows(
    struct window_switcher_application  *switcher,
    const struct compositor_window_tree *tree,
    const struct compositor_output      *focused_output
)
{
    if (switcher->settings->includes_all_workspaces) {
        return tree->all_windows;
    }

    return focused_output->windows_on_visible_workspace;
}

/* A plain copy: the tree's array would bring its free function along. */
static GPtrArray *
copy_window_list(
    GPtrArray *windows
)
{
    GPtrArray *copy = g_ptr_array_sized_new(windows->len);

    for (guint i = 0; i < windows->len; i++) {
        g_ptr_array_add(copy, g_ptr_array_index(windows, i));
    }

    return copy;
}

static GPtrArray *
windows_in_switching_order(
    struct window_switcher_application *switcher,
    GPtrArray                          *candidates
)
{
    GPtrArray *windows = copy_window_list(candidates);

    if (switcher->settings->orders_by_recent_focus) {
        g_ptr_array_sort_with_data(
            windows,
            compare_focus_recency,
            (void *)compositor_state_tracker_get_focus_history(switcher->compositor_state)
        );
    }

    return windows;
}

static void
destroy_switcher_entry(
    void *data
)
{
    struct window_switcher_entry *entry = data;

    g_free(entry->label);
    g_free(entry);
}

static struct window_switcher_entry *
create_switcher_entry(
    struct window_switcher_application *switcher,
    const struct compositor_window     *window
)
{
    struct window_switcher_entry    *entry  = g_new0(struct window_switcher_entry, 1);
    const struct window_label_values values = {
        .name       = window->application_name,
        .process_id = window->process_id,
        .title      = window->title,
        .workspace  = window->workspace_name,
    };

    entry->container_id = window->container_id;
    entry->label        = window_label_format_expand(switcher->settings->label_format, &values);

    return entry;
}

static GPtrArray *
create_switcher_entries(
    struct window_switcher_application *switcher,
    GPtrArray                          *windows
)
{
    GPtrArray *entries = g_ptr_array_new_with_free_func(destroy_switcher_entry);

    for (guint i = 0; i < windows->len; i++) {
        g_ptr_array_add(entries, create_switcher_entry(switcher, g_ptr_array_index(windows, i)));
    }

    return entries;
}

/* "next" preselects the window after the first, "previous" the last one. */
static guint
initial_selection(
    guint window_count,
    int   step
)
{
    if (step > 0) {
        return 1;
    }

    return window_count - 1;
}

static void
show_overlay_with_previews(
    struct window_switcher_application *switcher,
    GdkMonitor                         *monitor,
    GPtrArray                          *windows,
    int                                 step
)
{
    GPtrArray *entries = create_switcher_entries(switcher, windows);

    window_switcher_overlay_show(switcher->overlay, monitor, entries, initial_selection(windows->len, step));
    g_ptr_array_unref(entries);
    request_previews(switcher, windows);
}

/* With fewer than two windows there is nothing to switch to. */
static void
show_overlay_for(
    struct window_switcher_application *switcher,
    GdkMonitor                         *monitor,
    GPtrArray                          *candidates,
    int                                 step
)
{
    if (candidates->len < 2) {
        return;
    }

    GPtrArray *windows = windows_in_switching_order(switcher, candidates);

    show_overlay_with_previews(switcher, monitor, windows, step);
    g_ptr_array_unref(windows);
}

/* NULL while the focused output is unknown to the compositor or to GDK. */
static GdkMonitor *
monitor_of_output(
    const struct compositor_output *output
)
{
    if (output == NULL) {
        return NULL;
    }

    return monitor_connector_lookup_find(gdk_display_get_default(), output->output_name);
}

/* The overlay opens on the focused output, whichever windows it lists. */
static void
open_overlay(
    struct window_switcher_application *switcher,
    int                                 step
)
{
    const struct compositor_window_tree *tree    = compositor_state_tracker_get_window_tree(switcher->compositor_state);
    const struct compositor_output      *output  = compositor_window_tree_find_focused_output(tree);
    GdkMonitor                          *monitor = monitor_of_output(output);

    if (monitor == NULL) {
        return;
    }

    show_overlay_for(switcher, monitor, candidate_windows(switcher, tree, output), step);
}

static void
switch_windows(
    struct window_switcher_application *switcher,
    int                                 step
)
{
    if (window_switcher_overlay_is_visible(switcher->overlay)) {
        window_switcher_overlay_move_selection(switcher->overlay, step);
        return;
    }

    open_overlay(switcher, step);
}

static void
handle_key_binding_command(
    void       *user_data,
    const char *command
)
{
    if (strcmp(command, select_next_command) == 0) {
        switch_windows(user_data, 1);
    }

    if (strcmp(command, select_previous_command) == 0) {
        switch_windows(user_data, -1);
    }
}

static void
ignore_window_tree_update(
    void                                *user_data,
    const struct compositor_window_tree *tree
)
{}

static void
handle_compositor_gone(
    void *user_data
)
{
    struct window_switcher_application *switcher = user_data;

    g_application_quit(G_APPLICATION(switcher->gtk_application));
}

static void
apply_settings(
    struct window_switcher_application *switcher
)
{
    struct window_switcher_settings *settings           = switcher->settings;
    bool                             labels_show_titles = window_label_format_uses_title(settings->label_format);

    compositor_state_tracker_follow_title_changes(switcher->compositor_state, labels_show_titles);
    compositor_matched_appearance_refresh(switcher->appearance, settings->follows_sway_bar);
    window_switcher_overlay_set_preview_options(switcher->overlay, settings->shows_previews, settings->maximum_preview_width);
}

static void
reload_settings(
    struct window_switcher_application *switcher
)
{
    window_switcher_settings_destroy(switcher->settings);

    switcher->settings = window_switcher_settings_load(switcher->options->configuration_override_path);

    apply_settings(switcher);
}

static void
handle_compositor_configuration_reloaded(
    void *user_data
)
{
    reload_settings(user_data);
}

static const struct compositor_state_tracker_handler compositor_state_handler = {
    .handle_window_tree_updated               = ignore_window_tree_update,
    .handle_key_binding_command               = handle_key_binding_command,
    .handle_compositor_gone                   = handle_compositor_gone,
    .handle_compositor_configuration_reloaded = handle_compositor_configuration_reloaded,
};

static void
handle_window_chosen(
    void   *user_data,
    int64_t container_id
)
{
    struct window_switcher_application *switcher = user_data;

    compositor_state_tracker_focus_window(switcher->compositor_state, container_id);
}

static void
handle_overlay_closed(
    void *user_data
)
{
    struct window_switcher_application *switcher = user_data;

    g_ptr_array_set_size(switcher->pending_previews, 0);
}

static const struct window_switcher_overlay_handler overlay_handler = {
    .handle_window_chosen  = handle_window_chosen,
    .handle_overlay_closed = handle_overlay_closed,
};

static bool
connect_to_compositor(
    struct window_switcher_application *switcher
)
{
    GError *error = NULL;

    switcher->compositor_state = compositor_state_tracker_connect(
        switcher->options->compositor_socket_path,
        &compositor_state_handler,
        switcher,
        &error
    );

    if (switcher->compositor_state == NULL) {
        g_printerr("Cannot connect to the compositor: %s\n", error->message);
        g_error_free(error);
    }

    return switcher->compositor_state != NULL;
}

static void
create_interface(
    struct window_switcher_application *switcher,
    GdkDisplay                         *display
)
{
    switcher->appearance = compositor_matched_appearance_create(
        display,
        compositor_state_tracker_get_ipc_client(switcher->compositor_state),
        switcher->options->data_directory,
        "window-switcher",
        switcher->options->style_sheet_override_path
    );
    switcher->window_capture = toplevel_window_capture_create(display);
    switcher->overlay        = window_switcher_overlay_create(switcher->gtk_application, &overlay_handler, switcher);
}

/* Widgets can only be created once GtkApplication has started GTK. */
static void
handle_activate(
    GApplication *gtk_application,
    void         *user_data
)
{
    struct window_switcher_application *switcher = user_data;

    if (!connect_to_compositor(switcher)) {
        return;
    }

    create_interface(switcher, gdk_display_get_default());
    reload_settings(switcher);

    switcher->started_successfully = true;

    g_application_hold(gtk_application);
}

/* Previews are cancelled before the capture that owns them goes away, the appearance before its connection. */
static void
handle_shutdown(
    GApplication *gtk_application,
    void         *user_data
)
{
    struct window_switcher_application *switcher = user_data;

    g_ptr_array_set_size(switcher->pending_previews, 0);
    g_clear_pointer(&switcher->overlay, window_switcher_overlay_destroy);
    g_clear_pointer(&switcher->window_capture, toplevel_window_capture_destroy);
    g_clear_pointer(&switcher->appearance, compositor_matched_appearance_destroy);
    g_clear_pointer(&switcher->compositor_state, compositor_state_tracker_destroy);
    g_clear_pointer(&switcher->settings, window_switcher_settings_destroy);
}

bool
window_switcher_application_run(
    const struct window_switcher_options *options
)
{
    struct window_switcher_application switcher = {
        .options          = options,
        .gtk_application  = gtk_application_new(NULL, G_APPLICATION_NON_UNIQUE),
        .pending_previews = g_ptr_array_new_with_free_func(destroy_pending_preview),
    };

    g_signal_connect(switcher.gtk_application, "activate", G_CALLBACK(handle_activate), &switcher);
    g_signal_connect(switcher.gtk_application, "shutdown", G_CALLBACK(handle_shutdown), &switcher);
    g_application_run(G_APPLICATION(switcher.gtk_application), 0, NULL);
    g_ptr_array_unref(switcher.pending_previews);
    g_object_unref(switcher.gtk_application);

    return switcher.started_successfully;
}
