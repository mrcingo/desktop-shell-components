#include "workspace-window-bar-panel.h"
#include "layer-shell-window.h"
#include "window-label-format.h"

static const char container_id_data_key[] = "workspace-window-bar-container-id";

struct workspace_window_bar_panel {
    const struct workspace_window_bar_panel_handler *handler;
    void                                            *user_data;
    const struct workspace_window_bar_settings      *settings;
    GdkMonitor                                      *monitor;
    GtkWindow                                       *window;
    GtkWidget                                       *entry_row;
    char                                            *shown_state; // see describe_shown_state()
};

static char *
format_entry_label(
    struct workspace_window_bar_panel *panel,
    const struct compositor_window    *window
)
{
    const struct window_label_values values = {
        .name       = window->application_name,
        .process_id = window->process_id,
        .title      = window->title,
        .workspace  = window->workspace_name,
    };

    return window_label_format_expand(panel->settings->label_format, &values);
}

/* The entry's rectangle in ancestor's coordinates. */
static GdkRectangle
place_within(
    GtkWidget *entry_widget,
    GtkWidget *ancestor
)
{
    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, 0, 0);

    if (!gtk_widget_compute_bounds(entry_widget, ancestor, &bounds)) {
        return (GdkRectangle){0};
    }

    return (GdkRectangle){(int)bounds.origin.x, (int)bounds.origin.y, (int)bounds.size.width, (int)bounds.size.height};
}

/* GDK numbers buttons from 1: left, middle, right. */
static void
handle_entry_released(
    GtkGestureClick *click_gesture,
    int              press_count,
    double           pointer_x,
    double           pointer_y,
    void            *user_data
)
{
    struct workspace_window_bar_panel *panel        = user_data;
    GtkWidget                         *entry_widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(click_gesture));
    int64_t                           *container_id = g_object_get_data(G_OBJECT(entry_widget), container_id_data_key);
    int          mouse_button  = (int)gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(click_gesture)) - 1;
    GtkWidget   *menu_parent   = gtk_window_get_child(GTK_WINDOW(gtk_widget_get_root(entry_widget)));
    GdkRectangle menu_position = place_within(entry_widget, menu_parent);

    panel->handler->handle_window_clicked(panel->user_data, *container_id, mouse_button, menu_parent, &menu_position);
}

static void
remember_container_id(
    GtkWidget *entry_widget,
    int64_t    container_id
)
{
    int64_t *stored_id = g_new(int64_t, 1);

    *stored_id = container_id;

    g_object_set_data_full(G_OBJECT(entry_widget), container_id_data_key, stored_id, g_free);
}

/* Button 0 lets the gesture report every mouse button. */
static void
listen_to_every_mouse_button(
    struct workspace_window_bar_panel *panel,
    GtkWidget                         *entry_widget
)
{
    GtkGesture *click_gesture = gtk_gesture_click_new();

    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), 0);
    g_signal_connect(click_gesture, "released", G_CALLBACK(handle_entry_released), panel);
    gtk_widget_add_controller(entry_widget, GTK_EVENT_CONTROLLER(click_gesture));
}

static void
style_window_entry(
    GtkWidget                      *entry_button,
    const struct compositor_window *window
)
{
    gtk_widget_set_focusable(entry_button, FALSE);
    gtk_widget_add_css_class(entry_button, "workspace-window-bar-entry");

    if (window->has_keyboard_focus) {
        gtk_widget_add_css_class(entry_button, "focused-window");
    }
}

static GtkWidget *
create_window_entry(
    struct workspace_window_bar_panel *panel,
    const struct compositor_window    *window
)
{
    g_autofree char *entry_label = format_entry_label(panel, window);

    GtkWidget *entry_button = gtk_button_new_with_label(entry_label);

    remember_container_id(entry_button, window->container_id);
    listen_to_every_mouse_button(panel, entry_button);
    style_window_entry(entry_button, window);

    return entry_button;
}

static void
remove_all_entries(
    struct workspace_window_bar_panel *panel
)
{
    GtkWidget *entry;

    while ((entry = gtk_widget_get_first_child(panel->entry_row)) != NULL) {
        gtk_box_remove(GTK_BOX(panel->entry_row), entry);
    }
}

/*
 * A set height wins; otherwise a bottom bar matches the space sway's top bar
 * reserves, and a top bar takes its natural height, as it would otherwise
 * measure itself. The default size must change too, or GTK keeps 200px.
 */
static int
choose_panel_height(
    struct workspace_window_bar_panel *panel,
    int                                top_bar_height
)
{
    if (panel->settings->height > 0) {
        return panel->settings->height;
    }

    if (panel->settings->is_at_top) {
        return -1;
    }

    if (top_bar_height <= 0) {
        return -1; // no top bar to match: natural height
    }

    return top_bar_height;
}

static void
apply_panel_height(
    struct workspace_window_bar_panel *panel,
    int                                top_bar_height
)
{
    int panel_height = choose_panel_height(panel, top_bar_height);

    gtk_widget_set_size_request(panel->entry_row, -1, panel_height);
    gtk_window_set_default_size(panel->window, -1, panel_height);
}

static void
append_window_state(
    struct workspace_window_bar_panel *panel,
    GString                           *state,
    const struct compositor_window    *window
)
{
    g_autofree char *entry_label = format_entry_label(panel, window);

    g_string_append_printf(
        state,
        "%" G_GINT64_FORMAT ":%d:%s\n",
        window->container_id,
        window->has_keyboard_focus,
        entry_label
    );
}

/* Everything the panel draws, in one string, so an unchanged state skips the rebuild. */
static char *
describe_shown_state(
    struct workspace_window_bar_panel *panel,
    const struct compositor_output    *output
)
{
    if (output == NULL) {
        return g_strdup("");
    }

    GString *state = g_string_new(NULL);

    g_string_append_printf(state, "%d\n", output->space_reserved_above_workspace);

    for (guint i = 0; i < output->windows_on_visible_workspace->len; i++) {
        append_window_state(panel, state, g_ptr_array_index(output->windows_on_visible_workspace, i));
    }

    return g_string_free(state, FALSE);
}

static void
rebuild_entries(
    struct workspace_window_bar_panel *panel,
    const struct compositor_output    *output
)
{
    remove_all_entries(panel);

    if (output == NULL) {
        return;
    }

    apply_panel_height(panel, output->space_reserved_above_workspace);

    for (guint i = 0; i < output->windows_on_visible_workspace->len; i++) {
        gtk_box_append(
            GTK_BOX(panel->entry_row),
            create_window_entry(panel, g_ptr_array_index(output->windows_on_visible_workspace, i))
        );
    }
}

void
workspace_window_bar_panel_show_windows(
    struct workspace_window_bar_panel *panel,
    const struct compositor_output    *output
)
{
    char *new_state = describe_shown_state(panel, output);

    if (g_strcmp0(new_state, panel->shown_state) == 0) {
        g_free(new_state);
        return; // most tree updates change nothing this panel shows
    }

    g_free(panel->shown_state);

    panel->shown_state = new_state;

    rebuild_entries(panel, output);
}

const char *
workspace_window_bar_panel_get_output_name(
    struct workspace_window_bar_panel *panel
)
{
    return gdk_monitor_get_connector(panel->monitor);
}

static unsigned int
anchored_edges_for(
    const struct workspace_window_bar_settings *settings
)
{
    unsigned int sides = LAYER_SHELL_WINDOW_EDGE_LEFT | LAYER_SHELL_WINDOW_EDGE_RIGHT;

    if (settings->is_at_top) {
        return sides | LAYER_SHELL_WINDOW_EDGE_TOP;
    }

    return sides | LAYER_SHELL_WINDOW_EDGE_BOTTOM;
}

static GtkWindow *
create_edge_window(
    GtkApplication                             *application,
    GdkMonitor                                 *monitor,
    const struct workspace_window_bar_settings *settings
)
{
    const struct layer_shell_window_options window_options = {
        .surface_namespace = "workspace-window-bar",
        .layer             = GTK_LAYER_SHELL_LAYER_BOTTOM,
        .monitor           = monitor,
        .anchored_edges    = anchored_edges_for(settings),
        .space_reservation = LAYER_SHELL_WINDOW_RESERVE_OWN_SIZE,
        .keyboard_mode     = GTK_LAYER_SHELL_KEYBOARD_MODE_NONE,
    };

    GtkWindow *window = layer_shell_window_create(application, &window_options);

    gtk_widget_add_css_class(GTK_WIDGET(window), "workspace-window-bar");

    return window;
}

static void
attach_entry_row(
    struct workspace_window_bar_panel *panel
)
{
    panel->entry_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    gtk_window_set_child(panel->window, panel->entry_row);
    gtk_window_present(panel->window);
}

struct workspace_window_bar_panel *
workspace_window_bar_panel_create(
    GtkApplication                                  *application,
    GdkMonitor                                      *monitor,
    const struct workspace_window_bar_settings      *settings,
    const struct workspace_window_bar_panel_handler *handler,
    void                                            *user_data
)
{
    struct workspace_window_bar_panel *panel = g_new0(struct workspace_window_bar_panel, 1);

    *panel = (struct workspace_window_bar_panel){
        .handler   = handler,
        .user_data = user_data,
        .settings  = settings,
        .monitor   = g_object_ref(monitor),
        .window    = create_edge_window(application, monitor, settings),
    };

    attach_entry_row(panel);

    return panel;
}

void
workspace_window_bar_panel_destroy(
    struct workspace_window_bar_panel *panel
)
{
    if (panel == NULL) {
        return;
    }

    gtk_window_destroy(panel->window);
    g_object_unref(panel->monitor);
    g_free(panel->shown_state);
    g_free(panel);
}
