#include "window-switcher-overlay.h"
#include <math.h>

#include "alt-key-release-watcher.h"
#include "layer-shell-window.h"

enum {
    PREVIEW_MAXIMUM_WIDTH = 240,
    PREVIEW_MINIMUM_WIDTH = 96,

    /* Space around each preview: entry border and padding, and the gap between entries. */
    ENTRY_SURROUNDING_WIDTH = 2 * 2 + 2 * 4 + 12,

    /* The card's padding and border. */
    CARD_SURROUNDING_WIDTH = 2 * 24 + 2 * 2,

    ENTRY_SPACING = 12,
};

struct shown_entry {
    int64_t    container_id;
    GtkWidget *entry_widget;
    GtkWidget *preview_picture;
};

struct window_switcher_overlay {
    const struct window_switcher_overlay_handler *handler;
    void                                         *user_data;

    GtkWindow                      *window;
    GtkWidget                      *entry_row;
    struct alt_key_release_watcher *alt_release_watcher;
    guint                           deferred_close_source;

    GPtrArray *shown_entries; // struct shown_entry, empty while hidden
    guint      selected_index;

    bool is_visible;

    uint32_t preview_pixel_width;
    uint32_t preview_pixel_height;
};

static struct shown_entry *
selected_entry(
    struct window_switcher_overlay *overlay
)
{
    return g_ptr_array_index(overlay->shown_entries, overlay->selected_index);
}

static void
remove_all_entries(
    struct window_switcher_overlay *overlay
)
{
    GtkWidget *entry_widget;

    while ((entry_widget = gtk_widget_get_first_child(overlay->entry_row)) != NULL) {
        gtk_box_remove(GTK_BOX(overlay->entry_row), entry_widget);
    }

    g_ptr_array_set_size(overlay->shown_entries, 0);
}

static void
close_overlay(
    struct window_switcher_overlay *overlay
)
{
    if (!overlay->is_visible) {
        return;
    }

    overlay->is_visible = false;

    g_clear_handle_id(&overlay->deferred_close_source, g_source_remove);
    gtk_widget_set_visible(GTK_WIDGET(overlay->window), FALSE);
    remove_all_entries(overlay);
    overlay->handler->handle_overlay_closed(overlay->user_data);
}

static gboolean
close_overlay_when_idle(
    void *user_data
)
{
    struct window_switcher_overlay *overlay = user_data;

    overlay->deferred_close_source = 0;

    close_overlay(overlay);

    return G_SOURCE_REMOVE;
}

/* Hiding inside a key handler, while GDK prepares a frame for that key, makes GDK warn. */
static void
close_overlay_after_key_handling(
    struct window_switcher_overlay *overlay
)
{
    if (overlay->deferred_close_source != 0) {
        return;
    }

    overlay->deferred_close_source = g_idle_add(close_overlay_when_idle, overlay);
}

static void
choose_selected_window(
    struct window_switcher_overlay *overlay
)
{
    if (!overlay->is_visible) {
        return;
    }

    int64_t chosen_container_id = selected_entry(overlay)->container_id;

    close_overlay(overlay);
    overlay->handler->handle_window_chosen(overlay->user_data, chosen_container_id);
}

static void
select_entry(
    struct window_switcher_overlay *overlay,
    guint                           entry_index
)
{
    gtk_widget_remove_css_class(selected_entry(overlay)->entry_widget, "selected-window");

    overlay->selected_index = entry_index;

    gtk_widget_add_css_class(selected_entry(overlay)->entry_widget, "selected-window");
}

void
window_switcher_overlay_move_selection(
    struct window_switcher_overlay *overlay,
    int                             step
)
{
    if (!overlay->is_visible) {
        return;
    }

    int entry_count = (int)overlay->shown_entries->len;
    int moved_index = ((int)overlay->selected_index + step % entry_count + entry_count) % entry_count;

    select_entry(overlay, (guint)moved_index);
}

static void
select_next_entry(
    struct window_switcher_overlay *overlay
)
{
    window_switcher_overlay_move_selection(overlay, 1);
}

static void
select_previous_entry(
    struct window_switcher_overlay *overlay
)
{
    window_switcher_overlay_move_selection(overlay, -1);
}

typedef void (*overlay_key_action)(struct window_switcher_overlay *overlay);

/* Alt+Tab itself never arrives: the compositor turns it into a key binding command. */
static const struct {
    guint              key_value;
    overlay_key_action action;
} key_actions[] = {
    {GDK_KEY_Escape,       close_overlay_after_key_handling},
    {GDK_KEY_Return,       choose_selected_window          },
    {GDK_KEY_KP_Enter,     choose_selected_window          },
    {GDK_KEY_space,        choose_selected_window          },
    {GDK_KEY_Tab,          select_next_entry               },
    {GDK_KEY_Right,        select_next_entry               },
    {GDK_KEY_ISO_Left_Tab, select_previous_entry           },
    {GDK_KEY_Left,         select_previous_entry           },
};

static overlay_key_action
find_key_action(
    guint key_value
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(key_actions); i++) {
        if (key_actions[i].key_value == key_value) {
            return key_actions[i].action;
        }
    }

    return NULL;
}

static gboolean
handle_key_pressed(
    GtkEventControllerKey *key_controller,
    guint                  key_value,
    guint                  key_code,
    GdkModifierType        modifier_state,
    void                  *user_data
)
{
    overlay_key_action action = find_key_action(key_value);

    if (action == NULL) {
        return FALSE;
    }

    action(user_data);

    return TRUE;
}

static void
handle_alt_released(
    void *user_data
)
{
    choose_selected_window(user_data);
}

static void
handle_entry_clicked(
    GtkGestureClick *click_gesture,
    int              press_count,
    double           pointer_x,
    double           pointer_y,
    void            *user_data
)
{
    struct window_switcher_overlay *overlay      = user_data;
    GtkWidget                      *entry_widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(click_gesture));
    guint                           entry_index  = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(entry_widget), "entry-index"));

    select_entry(overlay, entry_index);
    choose_selected_window(overlay);
}

static GtkWidget *
create_preview_picture(
    int preview_width,
    int preview_height
)
{
    GtkWidget *preview_picture = gtk_picture_new();

    gtk_widget_add_css_class(preview_picture, "window-switcher-preview");
    gtk_picture_set_content_fit(GTK_PICTURE(preview_picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_size_request(preview_picture, preview_width, preview_height);

    return preview_picture;
}

static GtkWidget *
create_entry_name_label(
    const char *label_text,
    int         label_width
)
{
    GtkWidget *name_label = gtk_label_new(label_text);

    gtk_widget_add_css_class(name_label, "window-switcher-entry-name");
    gtk_label_set_ellipsize(GTK_LABEL(name_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(name_label), 1);
    gtk_widget_set_size_request(name_label, label_width, -1);

    return name_label;
}

static void
make_entry_clickable(
    struct window_switcher_overlay *overlay,
    GtkWidget                      *entry_widget,
    guint                           entry_index
)
{
    GtkGesture *click_gesture = gtk_gesture_click_new();

    g_object_set_data(G_OBJECT(entry_widget), "entry-index", GUINT_TO_POINTER(entry_index));
    g_signal_connect(click_gesture, "released", G_CALLBACK(handle_entry_clicked), overlay);
    gtk_widget_add_controller(entry_widget, GTK_EVENT_CONTROLLER(click_gesture));
}

static void
fill_entry_widget(
    struct shown_entry *shown,
    const char         *label_text,
    int                 label_width
)
{
    gtk_widget_add_css_class(shown->entry_widget, "window-switcher-entry");
    gtk_box_append(GTK_BOX(shown->entry_widget), shown->preview_picture);
    gtk_box_append(GTK_BOX(shown->entry_widget), create_entry_name_label(label_text, label_width));
}

static struct shown_entry *
create_shown_entry(
    const struct window_switcher_entry *entry,
    int                                 preview_width,
    int                                 preview_height
)
{
    struct shown_entry *shown = g_new0(struct shown_entry, 1);

    shown->container_id    = entry->container_id;
    shown->entry_widget    = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    shown->preview_picture = create_preview_picture(preview_width, preview_height);

    fill_entry_widget(shown, entry->label, preview_width);

    return shown;
}

static void
add_entry(
    struct window_switcher_overlay     *overlay,
    const struct window_switcher_entry *entry,
    int                                 preview_width,
    int                                 preview_height
)
{
    struct shown_entry *shown = create_shown_entry(entry, preview_width, preview_height);

    make_entry_clickable(overlay, shown->entry_widget, overlay->shown_entries->len);
    gtk_box_append(GTK_BOX(overlay->entry_row), shown->entry_widget);
    g_ptr_array_add(overlay->shown_entries, shown);
}

/* Previews shrink so that every window fits on 90% of the monitor's width. */
static int
preview_width_for(
    GdkMonitor *monitor,
    guint       entry_count
)
{
    GdkRectangle monitor_area;

    gdk_monitor_get_geometry(monitor, &monitor_area);

    int available_width = monitor_area.width * 9 / 10 - CARD_SURROUNDING_WIDTH;
    int preview_width   = available_width / (int)entry_count - ENTRY_SURROUNDING_WIDTH;

    return CLAMP(preview_width, PREVIEW_MINIMUM_WIDTH, PREVIEW_MAXIMUM_WIDTH);
}

/* Previews keep the monitor's aspect ratio, which most windows roughly share. */
static int
preview_height_for(
    GdkMonitor *monitor,
    int         preview_width
)
{
    GdkRectangle monitor_area;

    gdk_monitor_get_geometry(monitor, &monitor_area);

    return (int)((int64_t)preview_width * monitor_area.height / MAX(monitor_area.width, 1));
}

/* Fractional scales round up, so a preview is never captured smaller than it is drawn. */
static void
remember_preview_pixel_size(
    struct window_switcher_overlay *overlay,
    GdkMonitor                     *monitor,
    int                             preview_width,
    int                             preview_height
)
{
    double scale = gdk_monitor_get_scale(monitor);

    overlay->preview_pixel_width  = (uint32_t)ceil(preview_width * scale);
    overlay->preview_pixel_height = (uint32_t)ceil(preview_height * scale);
}

static void
add_entries_sized_for_monitor(
    struct window_switcher_overlay *overlay,
    GdkMonitor                     *monitor,
    GPtrArray                      *entries
)
{
    int preview_width  = preview_width_for(monitor, entries->len);
    int preview_height = preview_height_for(monitor, preview_width);

    remember_preview_pixel_size(overlay, monitor, preview_width, preview_height);

    for (guint i = 0; i < entries->len; i++) {
        add_entry(overlay, g_ptr_array_index(entries, i), preview_width, preview_height);
    }
}

void
window_switcher_overlay_get_preview_pixel_size(
    struct window_switcher_overlay *overlay,
    uint32_t                       *pixel_width,
    uint32_t                       *pixel_height
)
{
    *pixel_width  = overlay->preview_pixel_width;
    *pixel_height = overlay->preview_pixel_height;
}

static void
present_with_selection(
    struct window_switcher_overlay *overlay,
    GdkMonitor                     *monitor,
    guint                           selected_index
)
{
    overlay->selected_index = selected_index;
    overlay->is_visible     = true;

    select_entry(overlay, selected_index);
    gtk_layer_set_monitor(overlay->window, monitor);
    gtk_window_present(overlay->window);
}

void
window_switcher_overlay_show(
    struct window_switcher_overlay *overlay,
    GdkMonitor                     *monitor,
    GPtrArray                      *entries,
    guint                           selected_index
)
{
    g_return_if_fail(selected_index < entries->len);

    close_overlay(overlay);
    add_entries_sized_for_monitor(overlay, monitor, entries);
    present_with_selection(overlay, monitor, selected_index);
}

bool
window_switcher_overlay_is_visible(
    struct window_switcher_overlay *overlay
)
{
    return overlay->is_visible;
}

void
window_switcher_overlay_set_preview(
    struct window_switcher_overlay *overlay,
    int64_t                         container_id,
    GdkTexture                     *preview
)
{
    for (guint i = 0; i < overlay->shown_entries->len; i++) {
        struct shown_entry *shown = g_ptr_array_index(overlay->shown_entries, i);

        if (shown->container_id == container_id) {
            gtk_picture_set_paintable(GTK_PICTURE(shown->preview_picture), GDK_PAINTABLE(preview));
        }
    }
}

static GtkWindow *
create_overlay_window(
    GtkApplication *application
)
{
    const struct layer_shell_window_options window_options = {
        .surface_namespace = "window-switcher",
        .layer             = GTK_LAYER_SHELL_LAYER_OVERLAY,
        .monitor           = NULL,
        .anchored_edges    = 0,
        .space_reservation = LAYER_SHELL_WINDOW_RESERVE_NO_SPACE,
        .keyboard_mode     = GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE,
    };

    GtkWindow *window = layer_shell_window_create(application, &window_options);

    gtk_widget_add_css_class(GTK_WIDGET(window), "window-switcher");

    return window;
}

static void
build_card(
    struct window_switcher_overlay *overlay
)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    overlay->entry_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, ENTRY_SPACING);

    gtk_widget_set_name(card, "window-switcher-card");
    gtk_box_append(GTK_BOX(card), overlay->entry_row);
    gtk_window_set_child(overlay->window, card);
}

static void
listen_to_keys(
    struct window_switcher_overlay *overlay
)
{
    GtkEventController *key_controller = gtk_event_controller_key_new();

    gtk_event_controller_set_propagation_phase(key_controller, GTK_PHASE_CAPTURE);
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(handle_key_pressed), overlay);
    gtk_widget_add_controller(GTK_WIDGET(overlay->window), key_controller);
}

static struct window_switcher_overlay *
allocate_overlay(
    GtkApplication                               *application,
    const struct window_switcher_overlay_handler *handler,
    void                                         *user_data
)
{
    struct window_switcher_overlay *overlay = g_new0(struct window_switcher_overlay, 1);

    overlay->handler       = handler;
    overlay->user_data     = user_data;
    overlay->window        = create_overlay_window(application);
    overlay->shown_entries = g_ptr_array_new_with_free_func(g_free);

    return overlay;
}

struct window_switcher_overlay *
window_switcher_overlay_create(
    GtkApplication                               *application,
    const struct window_switcher_overlay_handler *handler,
    void                                         *user_data
)
{
    struct window_switcher_overlay *overlay = allocate_overlay(application, handler, user_data);

    overlay->alt_release_watcher = alt_key_release_watcher_create(overlay->window, handle_alt_released, overlay);

    build_card(overlay);
    listen_to_keys(overlay);

    return overlay;
}

void
window_switcher_overlay_destroy(
    struct window_switcher_overlay *overlay
)
{
    if (overlay == NULL) {
        return;
    }

    g_clear_handle_id(&overlay->deferred_close_source, g_source_remove);
    alt_key_release_watcher_destroy(overlay->alt_release_watcher);
    g_ptr_array_unref(overlay->shown_entries);
    gtk_window_destroy(overlay->window);
    g_free(overlay);
}
