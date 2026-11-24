#include "workspace-window-bar-panel.h"
#include "layer-shell-window.h"

static const char container_id_data_key[] = "workspace-window-bar-container-id";

struct workspace_window_bar_panel {
    const struct workspace_window_bar_panel_handler *handler;
    void                                            *user_data;
    GdkMonitor                                      *monitor;
    GtkWindow                                       *window;
    GtkWidget                                       *entry_row;

    char *shown_state; // see describe_shown_state()
};

static void
handle_entry_clicked(
    GtkButton *entry_button,
    void      *user_data
)
{
    struct workspace_window_bar_panel *panel        = user_data;
    int64_t                           *container_id = g_object_get_data(G_OBJECT(entry_button), container_id_data_key);

    panel->handler->handle_window_clicked(panel->user_data, *container_id);
}

/* The button keeps the id of the window it stands for, freed with the button. */
static void
remember_container_id(
    GtkWidget *entry_button,
    int64_t    container_id
)
{
    int64_t *stored_container_id = g_new(int64_t, 1);

    *stored_container_id = container_id;

    g_object_set_data_full(G_OBJECT(entry_button), container_id_data_key, stored_container_id, g_free);
}

static GtkWidget *
create_labelled_entry_button(
    const struct compositor_window *window
)
{
    char      *entry_label  = compositor_window_format_name_and_process_id(window);
    GtkWidget *entry_button = gtk_button_new_with_label(entry_label);

    g_free(entry_label);
    gtk_widget_set_focusable(entry_button, FALSE);
    gtk_widget_add_css_class(entry_button, "workspace-window-bar-entry");

    return entry_button;
}

static GtkWidget *
create_window_entry(
    struct workspace_window_bar_panel *panel,
    const struct compositor_window    *window
)
{
    GtkWidget *entry_button = create_labelled_entry_button(window);

    remember_container_id(entry_button, window->container_id);
    g_signal_connect(entry_button, "clicked", G_CALLBACK(handle_entry_clicked), panel);

    if (window->has_keyboard_focus) {
        gtk_widget_add_css_class(entry_button, "focused-window");
    }

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
 * The panel is as tall as the space the top bar reserves, so both bars
 * match. The default size must change too, or GTK keeps a 200px window.
 */
static void
match_top_bar_height(
    struct workspace_window_bar_panel *panel,
    int                                top_bar_height
)
{
    int panel_height = top_bar_height;

    if (panel_height <= 0) {
        panel_height = -1; // no top bar: use the natural height
    }

    gtk_widget_set_size_request(panel->entry_row, -1, panel_height);
    gtk_window_set_default_size(panel->window, -1, panel_height);
}

static void
append_window_state(
    GString                        *state,
    const struct compositor_window *window
)
{
    char *entry_label = compositor_window_format_name_and_process_id(window);

    g_string_append_printf(
        state,
        "%" G_GINT64_FORMAT ":%d:%s\n",
        window->container_id,
        window->has_keyboard_focus,
        entry_label
    );
    g_free(entry_label);
}

/* Everything the panel draws, in one string, so an unchanged state skips the rebuild. */
static char *
describe_shown_state(
    const struct compositor_output *output
)
{
    if (output == NULL) {
        return g_strdup("");
    }

    GString *state = g_string_new(NULL);

    g_string_append_printf(state, "%d\n", output->space_reserved_above_workspace);

    for (guint i = 0; i < output->windows_on_visible_workspace->len; i++) {
        append_window_state(state, g_ptr_array_index(output->windows_on_visible_workspace, i));
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

    match_top_bar_height(panel, output->space_reserved_above_workspace);

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
    char *new_state = describe_shown_state(output);

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

/* Along the bottom edge, below windows, keeping them out of its height. */
static GtkWindow *
create_bottom_edge_window(
    GtkApplication *application,
    GdkMonitor     *monitor
)
{
    const struct layer_shell_window_options window_options = {
        .surface_namespace = "workspace-window-bar",
        .layer             = GTK_LAYER_SHELL_LAYER_BOTTOM,
        .monitor           = monitor,
        .anchored_edges    = LAYER_SHELL_WINDOW_EDGE_BOTTOM | LAYER_SHELL_WINDOW_EDGE_LEFT | LAYER_SHELL_WINDOW_EDGE_RIGHT,
        .space_reservation = LAYER_SHELL_WINDOW_RESERVE_OWN_SIZE,
        .keyboard_mode     = GTK_LAYER_SHELL_KEYBOARD_MODE_NONE,
    };

    GtkWindow *window = layer_shell_window_create(application, &window_options);

    gtk_widget_add_css_class(GTK_WIDGET(window), "workspace-window-bar");

    return window;
}

/* The row holding one button per window fills the whole window. */
static GtkWidget *
attach_entry_row(
    GtkWindow *window
)
{
    GtkWidget *entry_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    gtk_window_set_child(window, entry_row);

    return entry_row;
}

struct workspace_window_bar_panel *
workspace_window_bar_panel_create(
    GtkApplication                                  *application,
    GdkMonitor                                      *monitor,
    const struct workspace_window_bar_panel_handler *handler,
    void                                            *user_data
)
{
    struct workspace_window_bar_panel *panel = g_new0(struct workspace_window_bar_panel, 1);

    *panel = (struct workspace_window_bar_panel){
        .handler   = handler,
        .user_data = user_data,
        .monitor   = g_object_ref(monitor),
        .window    = create_bottom_edge_window(application, monitor),
    };

    panel->entry_row = attach_entry_row(panel->window);

    gtk_window_present(panel->window);

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
