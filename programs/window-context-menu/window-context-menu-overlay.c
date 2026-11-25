#include "window-context-menu-overlay.h"
#include "layer-shell-window.h"
#include "window-context-menu-popover.h"

struct window_context_menu_overlay {
    GtkWindow                                        *window;
    GtkWidget                                        *menu_parent;
    struct window_context_menu_target                *target; // owned until the menu opens
    struct compositor_ipc_client                     *ipc_client;
    GdkRectangle                                      fallback_position;
    guint                                             pointer_wait_source;
    const struct window_context_menu_overlay_handler *handler;
    void                                             *user_data;
};

static void
handle_menu_closed(
    void *user_data
)
{
    struct window_context_menu_overlay *overlay = user_data;

    overlay->handler->handle_menu_closed(overlay->user_data);
}

static const struct window_context_menu_popover_handler popover_handler = {
    .handle_closed = handle_menu_closed,
};

/* How long to wait for the pointer's position before using the fallback position. */
static const guint pointer_wait_milliseconds = 250;

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
    window_context_menu_popover_show(
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
        .keyboard_mode     = GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND,
    };

    GtkWindow *window = layer_shell_window_create(application, &options);

    gtk_widget_add_css_class(GTK_WIDGET(window), "desktop-shell-component");
    gtk_widget_add_css_class(GTK_WIDGET(window), "window-context-menu-overlay");

    return window;
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

    listen_for_pointer(overlay);
    gtk_window_set_child(overlay->window, overlay->menu_parent);
    gtk_window_present(overlay->window);

    return overlay;
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
    window_context_menu_target_destroy(overlay->target);
    gtk_window_destroy(overlay->window);
    g_free(overlay);
}
