#include <gtk/gtk.h>

#include "compositor-state-tracker.h"
#include "per-monitor-objects.h"
#include "shared-style-sheets.h"
#include "workspace-window-bar-application.h"
#include "workspace-window-bar-panel.h"

struct workspace_window_bar_application {
    const struct workspace_window_bar_options *options;
    GtkApplication                            *gtk_application;
    bool                                       started_successfully;

    struct compositor_state_tracker *compositor_state;

    struct per_monitor_objects *panels; // struct workspace_window_bar_panel per monitor
};

static void
show_current_windows(
    struct workspace_window_bar_application *bar,
    struct workspace_window_bar_panel       *panel
)
{
    const struct compositor_window_tree *tree        = compositor_state_tracker_get_window_tree(bar->compositor_state);
    const char                          *output_name = workspace_window_bar_panel_get_output_name(panel);

    workspace_window_bar_panel_show_windows(panel, compositor_window_tree_find_output_by_name(tree, output_name));
}

static void
handle_window_clicked(
    void   *user_data,
    int64_t container_id
)
{
    struct workspace_window_bar_application *bar = user_data;

    compositor_state_tracker_focus_window(bar->compositor_state, container_id);
}

static const struct workspace_window_bar_panel_handler panel_handler = {
    .handle_window_clicked = handle_window_clicked,
};

static void *
create_panel_for_monitor(
    void       *user_data,
    GdkMonitor *monitor
)
{
    struct workspace_window_bar_application *bar = user_data;
    struct workspace_window_bar_panel
        *panel = workspace_window_bar_panel_create(bar->gtk_application, monitor, &panel_handler, bar);

    show_current_windows(bar, panel);

    return panel;
}

static void
destroy_panel(
    void *user_data,
    void *object
)
{
    workspace_window_bar_panel_destroy(object);
}

static const struct per_monitor_objects_handler panels_handler = {
    .create_object_for_monitor = create_panel_for_monitor,
    .destroy_object            = destroy_panel,
    .handle_monitors_changed   = NULL,
};

static void
handle_window_tree_updated(
    void                                *user_data,
    const struct compositor_window_tree *tree
)
{
    struct workspace_window_bar_application *bar = user_data;

    if (bar->panels == NULL) {
        return; // the panels are created after the first update
    }

    for (guint i = 0; i < per_monitor_objects_count(bar->panels); i++) {
        show_current_windows(bar, per_monitor_objects_get_object(bar->panels, i));
    }
}

static void
ignore_key_binding_command(
    void       *user_data,
    const char *command
)
{}

static void
handle_compositor_gone(
    void *user_data
)
{
    struct workspace_window_bar_application *bar = user_data;

    g_application_quit(G_APPLICATION(bar->gtk_application));
}

static const struct compositor_state_tracker_handler compositor_state_handler = {
    .handle_window_tree_updated = handle_window_tree_updated,
    .handle_key_binding_command = ignore_key_binding_command,
    .handle_compositor_gone     = handle_compositor_gone,
};

static bool
connect_to_compositor(
    struct workspace_window_bar_application *bar
)
{
    const char *socket_path = bar->options->compositor_socket_path;
    GError     *error       = NULL;

    bar->compositor_state = compositor_state_tracker_connect(socket_path, &compositor_state_handler, bar, &error);

    if (bar->compositor_state == NULL) {
        g_printerr("Cannot connect to the compositor: %s\n", error->message);
        g_error_free(error);
    }

    return bar->compositor_state != NULL;
}

static void
load_bar_style_sheets(
    struct workspace_window_bar_application *bar,
    GdkDisplay                              *display
)
{
    const char *data_directory            = bar->options->data_directory;
    const char *style_sheet_override_path = bar->options->style_sheet_override_path;

    shared_style_sheets_load_for_display(display, data_directory, "workspace-window-bar", style_sheet_override_path);
}

static void
create_panels(
    struct workspace_window_bar_application *bar,
    GdkDisplay                              *display
)
{
    bar->panels               = per_monitor_objects_create(display, &panels_handler, bar);
    bar->started_successfully = true;
}

/* Widgets can only be created once GtkApplication has started GTK. */
static void
handle_activate(
    GApplication *gtk_application,
    void         *user_data
)
{
    struct workspace_window_bar_application *bar     = user_data;
    GdkDisplay                              *display = gdk_display_get_default();

    if (!connect_to_compositor(bar)) {
        return;
    }

    load_bar_style_sheets(bar, display);
    create_panels(bar, display);

    /* Panels come and go with monitors; keep running without any. */
    g_application_hold(gtk_application);
}

/* Panels must go while GTK still runs. */
static void
handle_shutdown(
    GApplication *gtk_application,
    void         *user_data
)
{
    struct workspace_window_bar_application *bar = user_data;

    g_clear_pointer(&bar->panels, per_monitor_objects_destroy);
    g_clear_pointer(&bar->compositor_state, compositor_state_tracker_destroy);
}

bool
workspace_window_bar_application_run(
    const struct workspace_window_bar_options *options
)
{
    struct workspace_window_bar_application bar = {
        .options         = options,
        .gtk_application = gtk_application_new(NULL, G_APPLICATION_NON_UNIQUE),
    };

    g_signal_connect(bar.gtk_application, "activate", G_CALLBACK(handle_activate), &bar);
    g_signal_connect(bar.gtk_application, "shutdown", G_CALLBACK(handle_shutdown), &bar);
    g_application_run(G_APPLICATION(bar.gtk_application), 0, NULL);
    g_object_unref(bar.gtk_application);

    return bar.started_successfully;
}
