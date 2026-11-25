#include <stdbool.h>

#include "layer-shell-window.h"
#include "window-context-menu-overlay.h"
#include "window-context-menu-popover.h"

struct window_context_menu_overlay {
    GtkWindow                                        *window;
    GtkWidget                                        *menu_parent;
    struct window_context_menu_target                *target; // owned until the menu opens
    struct compositor_ipc_client                     *ipc_client;
    GdkRectangle                                      fallback_position;
    guint                                             pointer_wait_source;
    struct window_context_menu_popover               *popover; // NULL until open, and once closed
    struct window_context_menu_outside_click          outside_click;
    bool                                              was_clicked_outside;
    const struct window_context_menu_overlay_handler *handler;
    void                                             *user_data;
};

static const struct window_context_menu_outside_click *
report_outside_click(
    const struct window_context_menu_overlay *overlay
)
{
    if (!overlay->was_clicked_outside) {
        return NULL;
    }

    return &overlay->outside_click;
}

static void
handle_menu_closed(
    void *user_data
)
{
    struct window_context_menu_overlay *overlay = user_data;

    overlay->popover = NULL;

    overlay->handler->handle_menu_closed(overlay->user_data, report_outside_click(overlay));
}

static void
handle_screenshot_requested(
    void                                    *user_data,
    const struct window_context_menu_target *target
)
{
    struct window_context_menu_overlay *overlay = user_data;

    overlay->handler->handle_screenshot_requested(overlay->user_data, target);
}

static const struct window_context_menu_popover_handler popover_handler = {
    .handle_closed                 = handle_menu_closed,
    .handle_screenshot_requested   = handle_screenshot_requested,
    .parent_handles_outside_clicks = true,
    .keeps_menu_inside_parent      = true,
};

/* How long to wait for the pointer's position before using the fallback position. */
static const guint pointer_wait_milliseconds = 250;

/*
 * The overlay asks for the keyboard only on demand while it waits for the
 * pointer: sway does not tell a surface that holds the keyboard exclusively
 * where the pointer is, and the menu would open away from it. Once the menu
 * is placed, the overlay takes the keyboard so that Escape always closes it.
 */
static void
take_keyboard(
    GtkWindow *window
)
{
    gtk_layer_set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
}

/* Opens once: at the pointer when its position arrives, else at the fallback position. */
static void
open_menu_at(
    struct window_context_menu_overlay *overlay,
    const GdkRectangle                 *position
)
{
    if (overlay->target == NULL) {
        return; // already open
    }

    g_clear_handle_id(&overlay->pointer_wait_source, g_source_remove);
    take_keyboard(overlay->window);

    overlay->popover = window_context_menu_popover_show(
        overlay->menu_parent,
        position,
        g_steal_pointer(&overlay->target),
        overlay->ipc_client,
        &popover_handler,
        overlay
    );
}

static void
handle_pointer_entered(
    GtkEventControllerMotion *motion_controller,
    double                    x,
    double                    y,
    gpointer                  user_data
)
{
    const GdkRectangle pointer_position = {.x = (int)x, .y = (int)y, .width = 1, .height = 1};

    open_menu_at(user_data, &pointer_position);
}

static gboolean
open_menu_at_fallback_position(
    gpointer user_data
)
{
    struct window_context_menu_overlay *overlay = user_data;

    overlay->pointer_wait_source = 0;

    g_debug("the pointer's position did not arrive: menu opened at the window's title bar");
    open_menu_at(overlay, &overlay->fallback_position);

    return G_SOURCE_REMOVE;
}

/* sway tells a surface where the pointer is as soon as it appears under it. */
static void
wait_for_pointer_position(
    GtkWidget *menu_parent,
    gpointer   user_data
)
{
    struct window_context_menu_overlay *overlay = user_data;

    overlay->pointer_wait_source = g_timeout_add(pointer_wait_milliseconds, open_menu_at_fallback_position, overlay);
}

/* Outside the menu: the click closes it and is reported once it has closed. */
static void
handle_click_outside(
    GtkGestureClick *click_gesture,
    int              press_count,
    double           x,
    double           y,
    gpointer         user_data
)
{
    struct window_context_menu_overlay *overlay = user_data;

    g_debug("click outside the menu: button %u", gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(click_gesture)));

    overlay->was_clicked_outside = true;
    overlay->outside_click       = (struct window_context_menu_outside_click){
        gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(click_gesture)),
        (int)x,
        (int)y,
    };

    if (overlay->popover != NULL) {
        window_context_menu_popover_close(overlay->popover);
    }
}

/* Button 0 lets the gesture report every mouse button. */
static void
listen_for_clicks(
    struct window_context_menu_overlay *overlay
)
{
    GtkGesture *click_gesture = gtk_gesture_click_new();

    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), 0);
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(click_gesture), GTK_PHASE_CAPTURE);
    g_signal_connect(click_gesture, "pressed", G_CALLBACK(handle_click_outside), overlay);
    gtk_widget_add_controller(GTK_WIDGET(overlay->window), GTK_EVENT_CONTROLLER(click_gesture));
}

/* The overlay has the keyboard while the menu is open; Escape closes the menu. */
static gboolean
handle_key_pressed(
    GtkEventControllerKey *key_controller,
    guint                  key_value,
    guint                  key_code,
    GdkModifierType        modifiers,
    gpointer               user_data
)
{
    struct window_context_menu_overlay *overlay = user_data;

    g_debug("key pressed on the menu: %u", key_value);

    if (key_value != GDK_KEY_Escape) {
        return FALSE;
    }

    if (overlay->popover != NULL) {
        window_context_menu_popover_close(overlay->popover);
    }

    return TRUE;
}

static void
listen_for_escape(
    struct window_context_menu_overlay *overlay
)
{
    GtkEventController *key_controller = gtk_event_controller_key_new();

    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(handle_key_pressed), overlay);
    gtk_widget_add_controller(GTK_WIDGET(overlay->window), key_controller);
}

static void
listen_for_pointer(
    struct window_context_menu_overlay *overlay
)
{
    GtkEventController *motion_controller = gtk_event_controller_motion_new();

    g_signal_connect(motion_controller, "enter", G_CALLBACK(handle_pointer_entered), overlay);
    gtk_widget_add_controller(GTK_WIDGET(overlay->window), motion_controller);
    g_signal_connect(overlay->menu_parent, "map", G_CALLBACK(wait_for_pointer_position), overlay);
}

static GtkWindow *
create_overlay_window(
    GtkApplication *application,
    GdkMonitor     *monitor
)
{
    const struct layer_shell_window_options options = {
        .surface_namespace = "desktop-shell-window-context-menu",
        .layer             = GTK_LAYER_SHELL_LAYER_OVERLAY,
        .monitor           = monitor,
        .anchored_edges    = LAYER_SHELL_WINDOW_ALL_EDGES,
        .space_reservation = LAYER_SHELL_WINDOW_IGNORE_OTHER_RESERVATIONS,
        .keyboard_mode     = GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND, // see take_keyboard
    };

    GtkWindow *window = layer_shell_window_create(application, &options);

    gtk_widget_add_css_class(GTK_WIDGET(window), "desktop-shell-component");
    gtk_widget_add_css_class(GTK_WIDGET(window), "window-context-menu-overlay");

    return window;
}

static void
listen_for_input(
    struct window_context_menu_overlay *overlay
)
{
    listen_for_pointer(overlay);
    listen_for_clicks(overlay);
    listen_for_escape(overlay);
}

struct window_context_menu_overlay *
window_context_menu_overlay_show(
    GtkApplication                                   *application,
    GdkMonitor                                       *monitor,
    struct window_context_menu_target                *target,
    struct compositor_ipc_client                     *ipc_client,
    const GdkRectangle                               *fallback_position,
    const struct window_context_menu_overlay_handler *handler,
    void                                             *user_data
)
{
    struct window_context_menu_overlay *overlay = g_new0(struct window_context_menu_overlay, 1);

    *overlay = (struct window_context_menu_overlay){
        .window            = create_overlay_window(application, monitor),
        .menu_parent       = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0),
        .target            = target,
        .ipc_client        = ipc_client,
        .fallback_position = *fallback_position,
        .handler           = handler,
        .user_data         = user_data,
    };

    listen_for_input(overlay);
    gtk_window_set_child(overlay->window, overlay->menu_parent);
    gtk_window_present(overlay->window);

    return overlay;
}

/* A menu still closing must not report to an overlay that is gone. */
static void
stop_menu_reports(
    struct window_context_menu_overlay *overlay
)
{
    if (overlay->popover == NULL) {
        return;
    }

    window_context_menu_popover_stop_reporting(overlay->popover);
}

void
window_context_menu_overlay_destroy(
    struct window_context_menu_overlay *overlay
)
{
    if (overlay == NULL) {
        return;
    }

    g_clear_handle_id(&overlay->pointer_wait_source, g_source_remove);
    stop_menu_reports(overlay);
    window_context_menu_target_destroy(overlay->target);
    gtk_window_destroy(overlay->window);
    g_free(overlay);
}
