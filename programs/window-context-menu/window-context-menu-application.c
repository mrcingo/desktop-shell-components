#include <gtk/gtk.h>

#include "compositor-matched-appearance.h"
#include "compositor-state-tracker.h"
#include "monitor-connector-lookup.h"
#include "window-context-menu-application.h"
#include "window-context-menu-overlay.h"
#include "window-context-menu-settings.h"
#include "window-screenshot-capturing.h"

static const char open_menu_command[]  = "nop desktop-shell-window-context-menu";
static const char screenshot_command[] = "nop desktop-shell-window-context-menu screenshot";
static const char target_mark[]        = "_window-context-menu-target";

struct window_context_menu_application {
    const struct window_context_menu_options *options;
    GtkApplication                           *gtk_application;
    bool                                      started_successfully;

    struct compositor_state_tracker      *compositor_state;
    struct window_context_menu_settings  *settings;
    struct compositor_matched_appearance *appearance;
    struct window_context_menu_overlay   *overlay; // NULL while no menu is open
    struct window_screenshot_capturing   *screenshot_capturing;
    bool                                  menu_requested;
    bool                                  screenshot_requested;
};

static struct compositor_ipc_client *
ipc_client_of(
    struct window_context_menu_application *menu_program
)
{
    return compositor_state_tracker_get_ipc_client(menu_program->compositor_state);
}

static void
handle_menu_closed(
    void *user_data
)
{
    struct window_context_menu_application *menu_program = user_data;

    g_clear_pointer(&menu_program->overlay, window_context_menu_overlay_destroy);
}

/* The window's own content: covering windows and the menu never show. */
static void
take_screenshot(
    struct window_context_menu_application *menu_program,
    const char                             *capture_identifier,
    const char                             *application_name
)
{
    const struct window_screenshot_options options = {
        .copies_to_clipboard = menu_program->settings->copies_screenshots,
        .saves_file          = menu_program->settings->saves_screenshots,
        .folder              = menu_program->settings->screenshot_folder,
    };

    if (!window_screenshot_capturing_take(menu_program->screenshot_capturing, capture_identifier, application_name, &options)) {
        g_warning("This window cannot be captured");
    }
}

static void
handle_screenshot_requested(
    void                                    *user_data,
    const struct window_context_menu_target *target
)
{
    take_screenshot(user_data, target->capture_identifier, target->application_name);
}

static const struct window_context_menu_overlay_handler overlay_handler = {
    .handle_menu_closed          = handle_menu_closed,
    .handle_screenshot_requested = handle_screenshot_requested,
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

    menu_program->overlay = window_context_menu_overlay_show(
        menu_program->gtk_application,
        find_monitor(output),
        window_context_menu_target_create(window, "/proc"),
        ipc_client_of(menu_program),
        &menu_position,
        &overlay_handler,
        menu_program
    );
}

/* The mark only says which window was meant; it goes once read. */
static void
remove_target_mark(
    struct window_context_menu_application *menu_program
)
{
    g_autofree char *unmark = g_strdup_printf("unmark %s", target_mark);

    compositor_ipc_client_send_request(ipc_client_of(menu_program), COMPOSITOR_IPC_REQUEST_RUN_COMMAND, unmark, NULL, NULL);
}

static void
open_menu_for_marked_window(
    struct window_context_menu_application *menu_program,
    const struct compositor_window_tree    *tree
)
{
    const struct compositor_window *window = compositor_window_tree_find_marked_window(tree, target_mark);

    if (window == NULL) {
        return;
    }

    show_menu(menu_program, tree, window);
    remove_target_mark(menu_program);
}

/* Alt+Print Screen: the marked window, without any menu. */
static void
screenshot_marked_window(
    struct window_context_menu_application *menu_program,
    const struct compositor_window_tree    *tree
)
{
    const struct compositor_window *window = compositor_window_tree_find_marked_window(tree, target_mark);

    if (window == NULL) {
        return;
    }

    take_screenshot(menu_program, window->toplevel_capture_identifier, window->application_name);
    remove_target_mark(menu_program);
}

static void
handle_requested_screenshot(
    struct window_context_menu_application *menu_program,
    const struct compositor_window_tree    *tree
)
{
    if (!menu_program->screenshot_requested) {
        return;
    }

    menu_program->screenshot_requested = false;

    screenshot_marked_window(menu_program, tree);
}

static void
handle_requested_menu(
    struct window_context_menu_application *menu_program,
    const struct compositor_window_tree    *tree
)
{
    if (!menu_program->menu_requested) {
        return;
    }

    menu_program->menu_requested = false;

    open_menu_for_marked_window(menu_program, tree);
}

static void
handle_window_tree_updated(
    void                                *user_data,
    const struct compositor_window_tree *tree
)
{
    handle_requested_screenshot(user_data, tree);
    handle_requested_menu(user_data, tree);
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
request_for_marked_window(
    struct window_context_menu_application *menu_program,
    bool                                   *request_flag
)
{
    *request_flag = true;

    compositor_state_tracker_refresh_window_tree(menu_program->compositor_state);
}

/* The flag the binding asks to raise, or NULL. Screenshots work even when the menu is turned off. */
static bool *
find_request_flag(
    struct window_context_menu_application *menu_program,
    const char                             *command
)
{
    if (binding_runs_command(command, screenshot_command)) {
        return &menu_program->screenshot_requested;
    }

    if (!binding_runs_command(command, open_menu_command)) {
        return NULL;
    }

    if (!menu_program->settings->is_enabled) {
        return NULL;
    }

    return &menu_program->menu_requested;
}

/* Each part of the binding's command list must match exactly: "... menu" never runs "... menu screenshot". */
static void
handle_key_binding_command(
    void       *user_data,
    const char *command
)
{
    struct window_context_menu_application *menu_program = user_data;
    bool                                   *request_flag = find_request_flag(menu_program, command);

    if (request_flag == NULL) {
        return;
    }

    request_for_marked_window(menu_program, request_flag);
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

/* The screenshot taker keeps the clipboard, so it lives as long as the program. */
static void
create_display_resources(
    struct window_context_menu_application *menu_program
)
{
    create_appearance(menu_program);

    menu_program->screenshot_capturing = window_screenshot_capturing_create(gdk_display_get_default());
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

    create_display_resources(menu_program);
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
    g_clear_pointer(&menu_program->screenshot_capturing, window_screenshot_capturing_destroy);
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
