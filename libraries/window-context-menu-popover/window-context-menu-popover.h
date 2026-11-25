#ifndef WINDOW_CONTEXT_MENU_POPOVER_H
#define WINDOW_CONTEXT_MENU_POPOVER_H

#include <gtk/gtk.h>

#include "compositor-ipc-client.h"
#include "window-context-menu-target.h"

/*
 * The window menu: the window's title and process, then close, end, force
 * close, pause or resume the process, copy details, floating, fullscreen,
 * sticky, moving the window and a screenshot of it. See window-context-menu-actions.h.
 */

struct window_context_menu_popover_handler {
    /* The menu closed and removed itself; called after any chosen action ran. Optional. */
    void (*handle_closed)(void *user_data);
    /*
     * Screenshot was chosen; called once the menu is gone, before
     * handle_closed. target is valid only during the call. Optional: without
     * it the item is disabled.
     */
    void (*handle_screenshot_requested)(void *user_data, const struct window_context_menu_target *target);
};

/*
 * Shows the menu pointing at pointing_to, in parent's coordinates. Takes
 * ownership of target. ipc_client must outlive the menu.
 */
void window_context_menu_popover_show(
    GtkWidget                                        *parent,
    const GdkRectangle                               *pointing_to,
    struct window_context_menu_target                *target,
    struct compositor_ipc_client                     *ipc_client,
    const struct window_context_menu_popover_handler *handler,
    void                                             *user_data
);

#endif
