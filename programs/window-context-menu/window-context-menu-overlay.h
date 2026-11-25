#ifndef WINDOW_CONTEXT_MENU_OVERLAY_H
#define WINDOW_CONTEXT_MENU_OVERLAY_H

#include <gtk/gtk.h>

#include "compositor-ipc-client.h"
#include "window-context-menu-target.h"

/*
 * A transparent surface over one monitor that holds the window menu. The
 * menu opens at the pointer; if its position does not arrive quickly, at
 * fallback_position, in the monitor's pixels, such as just below the
 * clicked window's title bar.
 */

struct window_context_menu_overlay_handler {
    /* The menu closed; the overlay may be destroyed now. */
    void (*handle_menu_closed)(void *user_data);
    /* Screenshot was chosen; the menu is already gone. target is valid only during the call. */
    void (*handle_screenshot_requested)(void *user_data, const struct window_context_menu_target *target);
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
