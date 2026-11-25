#ifndef WINDOW_CONTEXT_MENU_OVERLAY_H
#define WINDOW_CONTEXT_MENU_OVERLAY_H

#include <gtk/gtk.h>

#include "compositor-ipc-client.h"
#include "window-context-menu-target.h"

/*
 * A transparent surface over one monitor that holds the window menu. sway
 * tells a new surface where the pointer is only once it moves, so the menu
 * opens at a position given in the monitor's pixels, such as just below
 * the clicked window's title bar.
 */

struct window_context_menu_overlay_handler {
    /* The menu closed; the overlay may be destroyed now. */
    void (*handle_menu_closed)(void *user_data);
};

struct window_context_menu_overlay;

/* Takes ownership of target. monitor may be NULL to let the compositor choose. */
struct window_context_menu_overlay *window_context_menu_overlay_show(
    GtkApplication                                   *application,
    GdkMonitor                                       *monitor,
    struct window_context_menu_target                *target,
    struct compositor_ipc_client                     *ipc_client,
    const GdkRectangle                               *menu_position,
    const struct window_context_menu_overlay_handler *handler,
    void                                             *user_data
);

void window_context_menu_overlay_destroy(struct window_context_menu_overlay *overlay);

#endif
