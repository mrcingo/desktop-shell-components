#ifndef WINDOW_CONTEXT_MENU_ACTIONS_H
#define WINDOW_CONTEXT_MENU_ACTIONS_H

#include <gdk/gdk.h>
#include <gio/gio.h>

#include "compositor-ipc-client.h"
#include "window-context-menu-target.h"

/*
 * The actions of a window menu, to insert into a widget as the "window"
 * group. Compositor actions address the window by its container ID; process
 * actions use the target's signal handle; copy actions fill the clipboard.
 *
 *   close, terminate-process, force-close, pause-process, resume-process,
 *   copy-process-id, copy-title, copy-application-name, copy-command-line,
 *   copy-working-directory, toggle-floating, toggle-fullscreen,
 *   toggle-sticky (stateful), center, move-to-workspace (string: a
 *   number), move-to-output (left, right, up, down), set-border (normal,
 *   pixel, none), set-layout (splith, splitv, tabbed, stacking),
 *   move-to-scratchpad, screenshot
 *
 * screenshot does nothing itself: whoever shows the menu watches the
 * action's "activate" signal and takes the screenshot once the menu is gone.
 *
 * Actions that cannot work for the target, such as signals for an X11
 * window, are disabled. target, ipc_client and clipboard must outlive the group.
 */

GActionGroup *window_context_menu_actions_create(
    const struct window_context_menu_target *target,
    struct compositor_ipc_client            *ipc_client,
    GdkClipboard                            *clipboard
);

/* Whether a move-to-workspace parameter is a plain workspace number. */
gboolean window_context_menu_is_workspace_number(const char *text);

#endif
