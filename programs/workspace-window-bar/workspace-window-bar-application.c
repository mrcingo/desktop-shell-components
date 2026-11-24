#include <gtk/gtk.h>

#include "compositor-matched-appearance.h"
#include "compositor-state-tracker.h"
#include "per-monitor-objects.h"
#include "window-label-format.h"
#include "workspace-window-bar-application.h"
#include "workspace-window-bar-panel.h"
#include "workspace-window-bar-settings.h"

struct workspace_window_bar_application {
    const struct workspace_window_bar_options *options;
    GtkApplication                            *gtk_application;
    bool                                       started_successfully;

    struct compositor_state_tracker      *compositor_state;
    struct workspace_window_bar_settings *settings;
    struct compositor_matched_appearance *appearance;
    struct per_monitor_objects           *panels; // struct workspace_window_bar_panel per monitor
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
perform_click_action(
    struct workspace_window_bar_application *bar,
    enum workspace_window_bar_click_action   action,
    int64_t                                  container_id
)
{
    if (action == WORKSPACE_WINDOW_BAR_CLICK_FOCUSES) {
        compositor_state_tracker_focus_window(bar->compositor_state, container_id);
    }

    if (action == WORKSPACE_WINDOW_BAR_CLICK_CLOSES) {
        compositor_state_tracker_close_window(bar->compositor_state, container_id);
    }
}

/* Buttons beyond the right one, such as side buttons, do nothing. */
static void
handle_window_clicked(
    void   *user_data,
    int64_t container_id,
    int     mouse_button
)
{
    struct workspace_window_bar_application *bar = user_data;

    if (mouse_button < 0) {
        return;
    }

    if (mouse_button >= WORKSPACE_WINDOW_BAR_MOUSE_BUTTON_COUNT) {
        return;
    }

    perform_click_action(bar, bar->settings->click_actions[mouse_button], container_id);
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
        *panel = workspace_window_bar_panel_create(bar->gtk_application, monitor, bar->settings, &panel_handler, bar);

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

/* Settings decide what the tracker must follow and how the panels look. */
static void
apply_settings(
    struct workspace_window_bar_application *bar
)
{
    bool labels_show_titles = window_label_format_uses_title(bar->settings->label_format);

    compositor_state_tracker_follow_title_changes(bar->compositor_state, labels_show_titles);
    compositor_matched_appearance_refresh(bar->appearance, bar->settings->follows_sway_bar);
}

/* Panels hold a pointer to the settings, so they are rebuilt around the new ones. */
static void
reload_settings(
    struct workspace_window_bar_application *bar
)
{
    GdkDisplay *display = gdk_display_get_default();

    g_clear_pointer(&bar->panels, per_monitor_objects_destroy);
    workspace_window_bar_settings_destroy(bar->settings);

    bar->settings = workspace_window_bar_settings_load(bar->options->configuration_override_path);
    bar->panels   = per_monitor_objects_create(display, &panels_handler, bar);

    apply_settings(bar);
}

static void
handle_compositor_configuration_reloaded(
    void *user_data
)
{
    reload_settings(user_data);
}

static const struct compositor_state_tracker_handler compositor_state_handler = {
    .handle_window_tree_updated               = handle_window_tree_updated,
    .handle_key_binding_command               = ignore_key_binding_command,
    .handle_compositor_gone                   = handle_compositor_gone,
    .handle_compositor_configuration_reloaded = handle_compositor_configuration_reloaded,
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
create_appearance(
    struct workspace_window_bar_application *bar,
    GdkDisplay                              *display
)
{
    bar->appearance = compositor_matched_appearance_create(
        display,
        compositor_state_tracker_get_ipc_client(bar->compositor_state),
        bar->options->data_directory,
        "workspace-window-bar",
        bar->options->style_sheet_override_path
    );
}

/* Widgets can only be created once GtkApplication has started GTK. */
static void
handle_activate(
    GApplication *gtk_application,
    void         *user_data
)
{
    struct workspace_window_bar_application *bar = user_data;

    if (!connect_to_compositor(bar)) {
        return;
    }

    create_appearance(bar, gdk_display_get_default());
    reload_settings(bar);

    bar->started_successfully = true;

    /* Panels come and go with monitors; keep running without any. */
    g_application_hold(gtk_application);
}

/* Panels must go while GTK still runs; the appearance goes before the connection it uses. */
static void
handle_shutdown(
    GApplication *gtk_application,
    void         *user_data
)
{
    struct workspace_window_bar_application *bar = user_data;

    g_clear_pointer(&bar->panels, per_monitor_objects_destroy);
    g_clear_pointer(&bar->appearance, compositor_matched_appearance_destroy);
    g_clear_pointer(&bar->compositor_state, compositor_state_tracker_destroy);
    g_clear_pointer(&bar->settings, workspace_window_bar_settings_destroy);
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
