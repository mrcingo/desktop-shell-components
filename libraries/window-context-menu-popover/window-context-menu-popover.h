#ifndef WINDOW_CONTEXT_MENU_POPOVER_H
#define WINDOW_CONTEXT_MENU_POPOVER_H

#include <stdbool.h>

#include <gtk/gtk.h>

#include "compositor-ipc-client.h"
#include "window-context-menu-target.h"

/*
 * The window menu: the window's title and process, then close, end, force
 * close, pause or resume the process, copy details, floating, fullscreen,
 * sticky and moving the window. See window-context-menu-actions.h.
 */

struct window_context_menu_popover_handler {
    /* The menu closed and removed itself; called after any chosen action ran. Optional. */
    void (*handle_closed)(void *user_data);
    /*
     * True when the parent catches clicks outside the menu itself and closes
     * it: the menu then does not close on its own, as the compositor would
     * otherwise swallow the click that closes it.
     */
    bool parent_handles_outside_clicks;
};

/* One open menu; it removes itself once closed. */
struct window_context_menu_popover;

/*
 * Shows the menu pointing at pointing_to, in parent's coordinates. Takes
 * ownership of target. ipc_client must outlive the menu. The result stays
 * valid until handle_closed has run, or until it is told to stop reporting.
 */
struct window_context_menu_popover *window_context_menu_popover_show(
    GtkWidget                                        *parent,
    const GdkRectangle                               *pointing_to,
    struct window_context_menu_target                *target,
    struct compositor_ipc_client                     *ipc_client,
    const struct window_context_menu_popover_handler *handler,
    void                                             *user_data
);

/* The handler's owner is going away: the menu removes itself without reporting, and the handle must not be used again. */
void window_context_menu_popover_stop_reporting(struct window_context_menu_popover *menu);

/* Closes the menu as a click outside would; it reports as on any close. */
void window_context_menu_popover_close(struct window_context_menu_popover *menu);

#endif
