#ifndef COMPOSITOR_STATE_TRACKER_H
#define COMPOSITOR_STATE_TRACKER_H

#include <stdbool.h>
#include <stdint.h>

#include <gio/gio.h>

#include "compositor-window-tree.h"
#include "window-focus-history.h"

/*
 * Keeps an up-to-date picture of the compositor: the window tree, refreshed
 * whenever windows, workspaces or outputs change, and the window focus
 * history. Also reports key binding commands, so a binding such as
 * "bindsym Mod1+Tab nop my-command" can drive a component without spawning
 * a process per key press.
 */

struct compositor_state_tracker;

struct compositor_state_tracker_handler {
    /* tree stays valid until the next update or until destroy. */
    void (*handle_window_tree_updated)(void *user_data, const struct compositor_window_tree *tree);
    /* The command of a key binding that just ran, such as "nop ...". */
    void (*handle_key_binding_command)(void *user_data, const char *command);
    /* The compositor is exiting or the connection broke. May destroy. */
    void (*handle_compositor_gone)(void *user_data);
};

struct compositor_state_tracker *compositor_state_tracker_connect(
    const char                                    *socket_path,
    const struct compositor_state_tracker_handler *handler,
    void                                          *user_data,
    GError                                       **error
);
struct compositor_state_tracker *compositor_state_tracker_create_for_connection(
    GSocketConnection                             *connection,
    const struct compositor_state_tracker_handler *handler,
    void                                          *user_data
);
void compositor_state_tracker_destroy(struct compositor_state_tracker *tracker);

/* NULL until the first tree has arrived. */
const struct compositor_window_tree *compositor_state_tracker_get_window_tree(struct compositor_state_tracker *tracker);
const struct window_focus_history   *compositor_state_tracker_get_focus_history(struct compositor_state_tracker *tracker);

void compositor_state_tracker_focus_window(struct compositor_state_tracker *tracker, int64_t container_id);

#endif
