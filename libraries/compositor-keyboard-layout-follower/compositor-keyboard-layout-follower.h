#ifndef COMPOSITOR_KEYBOARD_LAYOUT_FOLLOWER_H
#define COMPOSITOR_KEYBOARD_LAYOUT_FOLLOWER_H

#include <gio/gio.h>

/*
 * Follows the active keyboard layout through the compositor's IPC socket
 * and names it briefly ("es"), using the XKB rules file; a layout the rules
 * do not list keeps the compositor's own name ("Spanish").
 */

struct compositor_keyboard_layout_follower;

struct compositor_keyboard_layout_follower_handler {
    /* Only called when the name differs from the last one. */
    void (*handle_layout_changed)(void *user_data, const char *layout_name);
};

/* rules_file_path is normally /usr/share/X11/xkb/rules/evdev.xml. */
struct compositor_keyboard_layout_follower *compositor_keyboard_layout_follower_connect(
    const char                                               *socket_path,
    const char                                               *rules_file_path,
    const struct compositor_keyboard_layout_follower_handler *handler,
    void                                                     *user_data,
    GError                                                  **error
);

/* Takes over an already connected socket, as tests do. */
struct compositor_keyboard_layout_follower *compositor_keyboard_layout_follower_create_for_connection(
    GSocketConnection                                        *connection,
    const char                                               *rules_file_path,
    const struct compositor_keyboard_layout_follower_handler *handler,
    void                                                     *user_data
);

/* After the compositor goes away the last layout stays known. */
void compositor_keyboard_layout_follower_destroy(struct compositor_keyboard_layout_follower *follower);

/* NULL until the compositor has named a layout. */
const char *compositor_keyboard_layout_follower_get_layout_name(const struct compositor_keyboard_layout_follower *follower);

#endif
