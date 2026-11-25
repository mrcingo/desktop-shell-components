#include "window-snap-overlay.h"
#include "layer-shell-window.h"

enum {
    PREVIEW_OUTLINE_WIDTH = 2,
    LABEL_PADDING         = 8,
    LEFT_BUTTON           = 1,
    RIGHT_BUTTON          = 3,

    SHOWN_DELAY_MILLISECONDS = 50,
};

struct window_snap_overlay {
    GtkWindow                                *window;
    GtkWidget                                *drawing_area;
    guint                                     shown_timeout;
    bool                                      has_preview;
    GdkRectangle                              preview;
    char                                     *label;
    int                                       opacity;
    const struct window_snap_overlay_handler *handler;
    void                                     *user_data;
};

static void
fill_preview(
    struct window_snap_overlay *overlay,
    cairo_t                    *cairo,
    const GdkRGBA              *accent_color
)
{
    const GdkRectangle *preview = &overlay->preview;

    cairo_set_source_rgba(cairo, accent_color->red, accent_color->green, accent_color->blue, overlay->opacity / 100.0);
    cairo_rectangle(cairo, preview->x, preview->y, preview->width, preview->height);
    cairo_fill(cairo);
}

static void
outline_preview(
    struct window_snap_overlay *overlay,
    cairo_t                    *cairo,
    const GdkRGBA              *accent_color
)
{
    const GdkRectangle *preview = &overlay->preview;

    gdk_cairo_set_source_rgba(cairo, accent_color);
    cairo_set_line_width(cairo, PREVIEW_OUTLINE_WIDTH);
    cairo_rectangle(cairo, preview->x + 1, preview->y + 1, preview->width - 2, preview->height - 2);
    cairo_stroke(cairo);
}

static void draw_label(struct window_snap_overlay *overlay, cairo_t *cairo, const GdkRGBA *accent_color);

static void
draw_preview(
    struct window_snap_overlay *overlay,
    cairo_t                    *cairo,
    const GdkRGBA              *accent_color
)
{
    fill_preview(overlay, cairo, accent_color);
    outline_preview(overlay, cairo, accent_color);
    draw_label(overlay, cairo, accent_color);
}

/* Where the label goes: the middle of the preview. */
static graphene_rect_t
place_label(
    const struct window_snap_overlay *overlay,
    PangoLayout                      *layout
)
{
    int width  = 0;
    int height = 0;

    pango_layout_get_pixel_size(layout, &width, &height);

    return GRAPHENE_RECT_INIT(
        overlay->preview.x + (overlay->preview.width - width) / 2.0f,
        overlay->preview.y + (overlay->preview.height - height) / 2.0f,
        (float)width,
        (float)height
    );
}

static void
draw_label_box(
    cairo_t               *cairo,
    const graphene_rect_t *label_bounds,
    const GdkRGBA         *accent_color
)
{
    gdk_cairo_set_source_rgba(cairo, accent_color);
    cairo_rectangle(
        cairo,
        label_bounds->origin.x - LABEL_PADDING,
        label_bounds->origin.y - LABEL_PADDING / 2.0,
        label_bounds->size.width + 2 * LABEL_PADDING,
        label_bounds->size.height + LABEL_PADDING
    );
    cairo_fill(cairo);
}

/* The text takes the window's color, the accent's text color in the style sheet. */
static void
draw_label_text(
    struct window_snap_overlay *overlay,
    cairo_t                    *cairo,
    PangoLayout                *layout,
    const graphene_rect_t      *label_bounds
)
{
    GdkRGBA text_color;

    gtk_widget_get_color(GTK_WIDGET(overlay->window), &text_color);
    gdk_cairo_set_source_rgba(cairo, &text_color);
    cairo_move_to(cairo, label_bounds->origin.x, label_bounds->origin.y);
    pango_cairo_show_layout(cairo, layout);
}

/* The label sits in the middle of the preview, on a box of the accent color. */
static void
draw_label(
    struct window_snap_overlay *overlay,
    cairo_t                    *cairo,
    const GdkRGBA              *accent_color
)
{
    if (overlay->label == NULL) {
        return;
    }

    PangoLayout    *layout       = gtk_widget_create_pango_layout(overlay->drawing_area, overlay->label);
    graphene_rect_t label_bounds = place_label(overlay, layout);

    draw_label_box(cairo, &label_bounds, accent_color);
    draw_label_text(overlay, cairo, layout, &label_bounds);
    g_object_unref(layout);
}

/* The preview takes the drawing area's color, the accent color in the style sheet. */
static void
draw_overlay(
    GtkDrawingArea *drawing_area,
    cairo_t        *cairo,
    int             width,
    int             height,
    gpointer        user_data
)
{
    struct window_snap_overlay *overlay = user_data;
    GdkRGBA                     accent_color;

    if (!overlay->has_preview) {
        return;
    }

    gtk_widget_get_color(overlay->drawing_area, &accent_color);
    draw_preview(overlay, cairo, &accent_color);
}

static void
report_pointer_event(
    struct window_snap_overlay *overlay,
    GdkEvent                   *event,
    void (*report)(void *user_data, double x, double y)
)
{
    double x = 0;
    double y = 0;

    gdk_event_get_position(event, &x, &y);
    report(overlay->user_data, x, y);
}

static gboolean
handle_button_press(
    struct window_snap_overlay *overlay,
    GdkEvent                   *event
)
{
    if (gdk_button_event_get_button(event) == RIGHT_BUTTON) {
        overlay->handler->handle_cancelled(overlay->user_data);
    }

    return GDK_EVENT_STOP;
}

/* The press came from the compositor's cursor command; the user's own release ends it. */
static gboolean
handle_button_release(
    struct window_snap_overlay *overlay,
    GdkEvent                   *event
)
{
    if (gdk_button_event_get_button(event) == LEFT_BUTTON) {
        report_pointer_event(overlay, event, overlay->handler->handle_button_released);
    }

    return GDK_EVENT_STOP;
}

/* Keys go to the window, not to the drawing area, so the window listens for Escape. */
static gboolean
handle_key_pressed(
    GtkEventControllerKey *controller,
    guint                  keyval,
    guint                  keycode,
    GdkModifierType        state,
    gpointer               user_data
)
{
    struct window_snap_overlay *overlay = user_data;

    if (keyval == GDK_KEY_Escape) {
        overlay->handler->handle_cancelled(overlay->user_data);
    }

    return GDK_EVENT_STOP;
}

static bool
is_pointer_position_event(
    GdkEventType event_type
)
{
    return event_type == GDK_ENTER_NOTIFY || event_type == GDK_MOTION_NOTIFY;
}

static gboolean
dispatch_event(
    struct window_snap_overlay *overlay,
    GdkEvent                   *event,
    GdkEventType                event_type
)
{
    switch (event_type) {
    case GDK_BUTTON_PRESS:
        return handle_button_press(overlay, event);
    case GDK_BUTTON_RELEASE:
        return handle_button_release(overlay, event);
    default:
        return GDK_EVENT_PROPAGATE;
    }
}

/* Raw events: the press comes from the compositor's cursor command, so no gesture is in progress. */
static gboolean
handle_event(
    GtkEventControllerLegacy *controller,
    GdkEvent                 *event,
    gpointer                  user_data
)
{
    struct window_snap_overlay *overlay    = user_data;
    GdkEventType                event_type = gdk_event_get_event_type(event);

    if (is_pointer_position_event(event_type)) {
        report_pointer_event(overlay, event, overlay->handler->handle_pointer_moved);
        return GDK_EVENT_STOP;
    }

    return dispatch_event(overlay, event, event_type);
}

/* Keyboard focus is only taken once a drag starts, so a plain click changes no focus. */
static GtkWindow *
create_overlay_window(
    GtkApplication *application,
    GdkMonitor     *monitor
)
{
    const struct layer_shell_window_options options = {
        .surface_namespace = "desktop-shell-window-snap",
        .layer             = GTK_LAYER_SHELL_LAYER_OVERLAY,
        .monitor           = monitor,
        .anchored_edges    = LAYER_SHELL_WINDOW_ALL_EDGES,
        .space_reservation = LAYER_SHELL_WINDOW_IGNORE_OTHER_RESERVATIONS,
        .keyboard_mode     = GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE,
    };

    GtkWindow *window = layer_shell_window_create(application, &options);

    gtk_widget_add_css_class(GTK_WIDGET(window), "desktop-shell-component");
    gtk_widget_add_css_class(GTK_WIDGET(window), "window-snap-overlay");

    return window;
}

static GtkWidget *
create_drawing_area(
    struct window_snap_overlay *overlay
)
{
    GtkWidget          *drawing_area = gtk_drawing_area_new();
    GtkEventController *events       = gtk_event_controller_legacy_new();

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), draw_overlay, overlay, NULL);
    gtk_widget_add_css_class(drawing_area, "window-snap-preview");
    g_signal_connect(events, "event", G_CALLBACK(handle_event), overlay);
    gtk_widget_add_controller(drawing_area, events);

    return drawing_area;
}

/* The compositor maps the surface with its first frame; a little later it can take a press. */
static gboolean
report_shown(
    gpointer user_data
)
{
    struct window_snap_overlay *overlay = user_data;

    overlay->shown_timeout = 0;

    overlay->handler->handle_shown(overlay->user_data);

    return G_SOURCE_REMOVE;
}

static void
wait_until_shown(
    GtkWidget *window,
    gpointer   user_data
)
{
    struct window_snap_overlay *overlay = user_data;

    overlay->shown_timeout = g_timeout_add(SHOWN_DELAY_MILLISECONDS, report_shown, overlay);
}

static gboolean
destroy_window_later(
    gpointer window
)
{
    gtk_window_destroy(window);

    return G_SOURCE_REMOVE;
}

static void
hide_then_destroy_window(
    GtkWindow *window
)
{
    gtk_widget_set_visible(GTK_WIDGET(window), FALSE);
    gdk_display_flush(gdk_display_get_default());
    g_idle_add(destroy_window_later, window);
}

static void
listen_for_escape(
    struct window_snap_overlay *overlay
)
{
    GtkEventController *keys = gtk_event_controller_key_new();

    g_signal_connect(keys, "key-pressed", G_CALLBACK(handle_key_pressed), overlay);
    gtk_widget_add_controller(GTK_WIDGET(overlay->window), keys);
}

static void
show_overlay(
    struct window_snap_overlay *overlay
)
{
    listen_for_escape(overlay);
    g_signal_connect(overlay->window, "map", G_CALLBACK(wait_until_shown), overlay);
    gtk_window_set_child(overlay->window, overlay->drawing_area);
    gtk_window_present(overlay->window);
}

struct window_snap_overlay *
window_snap_overlay_create(
    GtkApplication                           *application,
    GdkMonitor                               *monitor,
    const struct window_snap_overlay_handler *handler,
    void                                     *user_data
)
{
    struct window_snap_overlay *overlay = g_new0(struct window_snap_overlay, 1);

    overlay->handler      = handler;
    overlay->user_data    = user_data;
    overlay->window       = create_overlay_window(application, monitor);
    overlay->drawing_area = create_drawing_area(overlay);

    show_overlay(overlay);

    return overlay;
}

/*
 * The overlay may be destroyed from its own event handler, so the window
 * goes once that returns; it is hidden at once, and the compositor learns
 * so before any command that follows, as the overlay holds the keyboard.
 */
void
window_snap_overlay_destroy(
    struct window_snap_overlay *overlay
)
{
    if (overlay == NULL) {
        return;
    }

    g_clear_handle_id(&overlay->shown_timeout, g_source_remove);
    hide_then_destroy_window(overlay->window);
    g_free(overlay->label);
    g_free(overlay);
}

void
window_snap_overlay_show_preview(
    struct window_snap_overlay *overlay,
    const GdkRectangle         *preview,
    const char                 *label,
    int                         opacity
)
{
    overlay->has_preview = preview != NULL;
    overlay->preview     = preview != NULL ? *preview : (GdkRectangle){0};
    overlay->opacity     = opacity;

    g_free(overlay->label);

    overlay->label = g_strdup(label);

    gtk_widget_queue_draw(overlay->drawing_area);
}

void
window_snap_overlay_take_keyboard(
    struct window_snap_overlay *overlay
)
{
    gtk_layer_set_keyboard_mode(overlay->window, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
}
