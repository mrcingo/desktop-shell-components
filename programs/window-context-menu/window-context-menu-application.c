#include <inttypes.h>

#include <gtk/gtk.h>

#include "compositor-matched-appearance.h"
#include "compositor-state-tracker.h"
#include "monitor-connector-lookup.h"
#include "window-context-menu-application.h"
#include "window-context-menu-overlay.h"
#include "window-context-menu-settings.h"

static const char open_menu_command[] = "nop desktop-shell-window-context-menu";
static const char target_mark[]       = "_window-context-menu-target";

struct window_context_menu_application {
    const struct window_context_menu_options *options;
    GtkApplication                           *gtk_application;
    bool                                      started_successfully;

    struct compositor_state_tracker      *compositor_state;
    struct window_context_menu_settings  *settings;
    struct compositor_matched_appearance *appearance;
    struct window_context_menu_overlay   *overlay;        // NULL while no menu is open
    int64_t                               menu_window_id; // the window the open menu is for
    int                                   menu_origin_x;  // top-left corner of its monitor, in layout pixels
    int                                   menu_origin_y;
    bool                                  menu_requested;
};

static struct compositor_ipc_client *
ipc_client_of(
    struct window_context_menu_application *menu_program
)
{
    return compositor_state_tracker_get_ipc_client(menu_program->compositor_state);
}

static void show_menu(
    struct window_context_menu_application *menu_program,
    const struct compositor_window_tree    *tree,
    const struct compositor_window         *window
);

static bool
is_within(
    int value,
    int start,
    int length
)
{
    if (value < start) {
        return false;
    }

    return value < start + length;
}

/* The band of border along the sides and bottom, at least a pixel wide. */
static bool
is_on_border(
    const struct compositor_window *window,
    int                             x,
    int                             y
)
{
    int band = MAX(window->border_width, 1);

    if (x < window->content_x + band) {
        return true;
    }

    if (x >= window->content_x + window->content_width - band) {
        return true;
    }

    return y >= window->content_y + window->content_height - band;
}

/* Where right-clicks open the menu: the title bar and the border, as the compositor's binding does. */
static bool
has_menu_handle_at(
    const struct compositor_window *window,
    int                             x,
    int                             y
)
{
    int top = window->content_y - window->title_bar_height;

    if (!is_within(x, window->content_x, window->content_width)) {
        return false;
    }

    if (!is_within(y, top, window->content_y + window->content_height - top)) {
        return false;
    }

    return y < window->content_y || is_on_border(window, x, y);
}

/* The topmost window with its title bar or border at the point; floating windows come last, on top. */
static const struct compositor_window *
find_window_handle_on_output(
    const struct compositor_output *output,
    int                             x,
    int                             y
)
{
    for (guint i = output->windows_on_visible_workspace->len; i > 0; i--) {
        const struct compositor_window *window = g_ptr_array_index(output->windows_on_visible_workspace, i - 1);

        if (has_menu_handle_at(window, x, y)) {
            return window;
        }
    }

    return NULL;
}

static const struct compositor_window *
find_window_handle_at(
    const struct compositor_window_tree *tree,
    int                                  x,
    int                                  y
)
{
    for (guint i = 0; i < tree->outputs->len; i++) {
        const struct compositor_window *window = find_window_handle_on_output(g_ptr_array_index(tree->outputs, i), x, y);

        if (window != NULL) {
            return window;
        }
    }

    return NULL;
}

/* A right-click outside an open menu, on another title bar or border, opens that window's menu, as on Windows. */
static void
reopen_menu_for_click(
    struct window_context_menu_application         *menu_program,
    const struct window_context_menu_outside_click *click
)
{
    const struct compositor_window_tree *tree   = compositor_state_tracker_get_window_tree(menu_program->compositor_state);
    int                                  x      = menu_program->menu_origin_x + click->x;
    int                                  y      = menu_program->menu_origin_y + click->y;
    const struct compositor_window      *window = find_window_handle_at(tree, x, y);

    if (window != NULL) {
        show_menu(menu_program, tree, window);
    }
}

static void
handle_menu_closed(
    void                                           *user_data,
    const struct window_context_menu_outside_click *click
)
{
    struct window_context_menu_application *menu_program = user_data;

    g_debug("menu closed");
    g_clear_pointer(&menu_program->overlay, window_context_menu_overlay_destroy);

    if (click == NULL) {
        return;
    }

    if (click->mouse_button == GDK_BUTTON_SECONDARY) {
        reopen_menu_for_click(menu_program, click);
    }
}

static const struct window_context_menu_overlay_handler overlay_handler = {
    .handle_menu_closed = handle_menu_closed,
};

/* NULL lets the compositor choose, when the monitor is unknown to GTK. */
static GdkMonitor *
find_monitor(
    const struct compositor_output *output
)
{
    if (output == NULL) {
        return NULL;
    }

    return monitor_connector_lookup_find(gdk_display_get_default(), output->output_name);
}

/* Just below the title bar at the window's left edge, in the monitor's pixels. */
static GdkRectangle
place_menu_below_title_bar(
    const struct compositor_output *output,
    const struct compositor_window *window
)
{
    if (output == NULL) {
        return (GdkRectangle){.width = 1, .height = 1};
    }

    return (GdkRectangle){window->content_x - output->output_x, window->content_y - output->output_y, 1, 1};
}

static void
remember_menu_origin(
    struct window_context_menu_application *menu_program,
    const struct compositor_output         *output
)
{
    if (output == NULL) {
        return;
    }

    menu_program->menu_origin_x = output->output_x;
    menu_program->menu_origin_y = output->output_y;
}

/* The menu opens on the monitor showing the window, which is where the click was. */
static void
show_menu(
    struct window_context_menu_application *menu_program,
    const struct compositor_window_tree    *tree,
    const struct compositor_window         *window
)
{
    const struct compositor_output *output = compositor_window_tree_find_output_showing_window(tree, window->container_id);
    GdkRectangle                    menu_position = place_menu_below_title_bar(output, window);

    g_clear_pointer(&menu_program->overlay, window_context_menu_overlay_destroy);
    g_debug("menu opened for window %" PRId64, window->container_id);

    remember_menu_origin(menu_program, output);

    menu_program->menu_window_id = window->container_id;
    menu_program->overlay        = window_context_menu_overlay_show(
        menu_program->gtk_application,
        find_monitor(output),
        window_context_menu_target_create(window, "/proc"),
        ipc_client_of(menu_program),
        &menu_position,
        &overlay_handler,
        menu_program
    );
}

/* The mark only says which window was clicked; it goes once read. */
static void
open_menu_for_marked_window(
    struct window_context_menu_application *menu_program,
    const struct compositor_window_tree    *tree
)
{
    const struct compositor_window *window = compositor_window_tree_find_marked_window(tree, target_mark);

    g_autofree char *unmark = g_strdup_printf("unmark %s", target_mark);

    if (window == NULL) {
        return;
    }

    show_menu(menu_program, tree, window);
    compositor_ipc_client_send_request(ipc_client_of(menu_program), COMPOSITOR_IPC_REQUEST_RUN_COMMAND, unmark, NULL, NULL);
}

/* A menu for a window that has closed meanwhile has nothing left to act on. */
static void
close_menu_of_closed_window(
    struct window_context_menu_application *menu_program,
    const struct compositor_window_tree    *tree
)
{
    if (menu_program->overlay == NULL) {
        return;
    }

    if (compositor_window_tree_find_window(tree, menu_program->menu_window_id) != NULL) {
        return;
    }

    g_debug("menu closed: its window is gone");
    g_clear_pointer(&menu_program->overlay, window_context_menu_overlay_destroy);
}

static void
handle_window_tree_updated(
    void                                *user_data,
    const struct compositor_window_tree *tree
)
{
    struct window_context_menu_application *menu_program = user_data;

    close_menu_of_closed_window(menu_program, tree);

    if (!menu_program->menu_requested) {
        return;
    }

    menu_program->menu_requested = false;

    open_menu_for_marked_window(menu_program, tree);
}

/* A binding's command is a list such as "mark --add X, nop Y"; any part may match. */
static bool
binding_runs_command(
    const char *binding_command,
    const char *wanted_command
)
{
    g_auto(GStrv) parts = g_strsplit_set(binding_command, ",;", -1);

    for (char **part = parts; *part != NULL; part++) {
        if (g_strcmp0(g_strstrip(*part), wanted_command) == 0) {
            return true;
        }
    }

    return false;
}

/* The mark is set by the same binding, so the next tree shows it. */
static void
handle_key_binding_command(
    void       *user_data,
    const char *command
)
{
    struct window_context_menu_application *menu_program = user_data;

    if (!binding_runs_command(command, open_menu_command)) {
        return;
    }

    if (!menu_program->settings->is_enabled) {
        return;
    }

    menu_program->menu_requested = true;

    compositor_state_tracker_refresh_window_tree(menu_program->compositor_state);
}

static void
handle_compositor_gone(
    void *user_data
)
{
    struct window_context_menu_application *menu_program = user_data;

    g_application_quit(G_APPLICATION(menu_program->gtk_application));
}

static void
reload_settings(
    struct window_context_menu_application *menu_program
)
{
    window_context_menu_settings_destroy(menu_program->settings);

    menu_program->settings = window_context_menu_settings_load(menu_program->options->configuration_override_path);

    compositor_matched_appearance_refresh(menu_program->appearance, menu_program->settings->follows_sway_bar);
}

static void
handle_compositor_configuration_reloaded(
    void *user_data
)
{
    struct window_context_menu_application *menu_program = user_data;

    replaced_executable_restart_if_replaced(menu_program->options->executable_restart);
    reload_settings(menu_program);
}

static const struct compositor_state_tracker_handler compositor_state_handler = {
    .handle_window_tree_updated               = handle_window_tree_updated,
    .handle_key_binding_command               = handle_key_binding_command,
    .handle_compositor_gone                   = handle_compositor_gone,
    .handle_compositor_configuration_reloaded = handle_compositor_configuration_reloaded,
};

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
    struct window_context_menu_application *menu_program
)
{
    GError *error = NULL;

    menu_program->compositor_state = compositor_state_tracker_connect(
        menu_program->options->compositor_socket_path,
        &compositor_state_handler,
        menu_program,
        &error
    );

    if (menu_program->compositor_state == NULL) {
        report_connection_failure(error);
        return false;
    }

    return true;
}

static void
create_appearance(
    struct window_context_menu_application *menu_program
)
{
    menu_program->appearance = compositor_matched_appearance_create(
        gdk_display_get_default(),
        ipc_client_of(menu_program),
        menu_program->options->data_directory,
        "window-context-menu",
        menu_program->options->style_sheet_override_path
    );
}

/* Widgets can only be created once GtkApplication has started GTK. */
static void
handle_activate(
    GApplication *gtk_application,
    void         *user_data
)
{
    struct window_context_menu_application *menu_program = user_data;

    if (!connect_to_compositor(menu_program)) {
        return;
    }

    menu_program->started_successfully = true;

    create_appearance(menu_program);
    reload_settings(menu_program);

    /* No window stays open between menus; keep running anyway. */
    g_application_hold(gtk_application);
}

/* The overlay must go while GTK still runs; the appearance goes before the connection it uses. */
static void
handle_shutdown(
    GApplication *gtk_application,
    void         *user_data
)
{
    struct window_context_menu_application *menu_program = user_data;

    g_clear_pointer(&menu_program->overlay, window_context_menu_overlay_destroy);
    g_clear_pointer(&menu_program->appearance, compositor_matched_appearance_destroy);
    g_clear_pointer(&menu_program->compositor_state, compositor_state_tracker_destroy);
    g_clear_pointer(&menu_program->settings, window_context_menu_settings_destroy);
}

bool
window_context_menu_application_run(
    const struct window_context_menu_options *options
)
{
    struct window_context_menu_application menu_program = {
        .options         = options,
        .gtk_application = gtk_application_new(NULL, G_APPLICATION_NON_UNIQUE),
    };

    g_signal_connect(menu_program.gtk_application, "activate", G_CALLBACK(handle_activate), &menu_program);
    g_signal_connect(menu_program.gtk_application, "shutdown", G_CALLBACK(handle_shutdown), &menu_program);
    g_application_run(G_APPLICATION(menu_program.gtk_application), 0, NULL);
    g_object_unref(menu_program.gtk_application);

    return menu_program.started_successfully;
}
