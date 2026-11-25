#include "window-context-menu-overlay.h"
#include "layer-shell-window.h"
#include "window-context-menu-popover.h"

struct window_context_menu_overlay {
    GtkWindow                                        *window;
    GtkWidget                                        *menu_parent;
    struct window_context_menu_target                *target; // owned until the menu opens
    struct compositor_ipc_client                     *ipc_client;
    GdkRectangle                                      menu_position;
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

/* A popover can only open once its parent is on screen. */
static void
open_menu_once_shown(
    GtkWidget *menu_parent,
    gpointer   user_data
)
{
    struct window_context_menu_overlay *overlay = user_data;

    if (overlay->target == NULL) {
        return; // mapped again; the menu is already open
    }

    window_context_menu_popover_show(
        overlay->menu_parent,
        &overlay->menu_position,
        g_steal_pointer(&overlay->target),
        overlay->ipc_client,
        &popover_handler,
        overlay
    );
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
    const GdkRectangle                               *menu_position,
    const struct window_context_menu_overlay_handler *handler,
    void                                             *user_data
)
{
    struct window_context_menu_overlay *overlay = g_new0(struct window_context_menu_overlay, 1);

    *overlay = (struct window_context_menu_overlay){
        .window        = create_overlay_window(application, monitor),
        .menu_parent   = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0),
        .target        = target,
        .ipc_client    = ipc_client,
        .menu_position = *menu_position,
        .handler       = handler,
        .user_data     = user_data,
    };

    g_signal_connect(overlay->menu_parent, "map", G_CALLBACK(open_menu_once_shown), overlay);
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

    window_context_menu_target_destroy(overlay->target);
    gtk_window_destroy(overlay->window);
    g_free(overlay);
}
