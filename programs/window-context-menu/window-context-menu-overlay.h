#ifndef WINDOW_CONTEXT_MENU_OVERLAY_H
#define WINDOW_CONTEXT_MENU_OVERLAY_H

#include <gtk/gtk.h>

#include "compositor-ipc-client.h"
#include "window-context-menu-target.h"

/*
 * A transparent surface over one monitor that holds the window menu. While
 * the menu is open the surface has the keyboard, as Windows menus do, so
 * Escape and the arrow keys always reach it, and clicks outside the menu
 * land on the surface, which closes the menu and reports the click. The
 * menu opens at the pointer; if its position does not arrive quickly, at
 * fallback_position, in the monitor's pixels, such as just below the
 * clicked window's title bar.
 */

/* A click outside the open menu, in the monitor's pixels; it closes the menu. */
struct window_context_menu_outside_click {
    unsigned int mouse_button; // 1 left, 2 middle, 3 right
    int          x;
    int          y;
};

struct window_context_menu_overlay_handler {
    /*
     * The menu closed; the overlay may be destroyed now. click is the click
     * outside the menu that closed it, or NULL when it closed otherwise.
     */
    void (*handle_menu_closed)(void *user_data, const struct window_context_menu_outside_click *click);
};

struct window_context_menu_overlay;

/* Takes ownership of target. monitor may be NULL to let the compositor choose. */
struct window_context_menu_overlay *window_context_menu_overlay_show(
    GtkApplication                                   *application,
    GdkMonitor                                       *monitor,
    struct window_context_menu_target                *target,
    struct compositor_ipc_client                     *ipc_client,
    const GdkRectangle                               *fallback_position,
    const struct window_context_menu_overlay_handler *handler,
    void                                             *user_data
);

void window_context_menu_overlay_destroy(struct window_context_menu_overlay *overlay);

#endif
