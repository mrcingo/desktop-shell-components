#ifndef LOGIN_GREETER_KEYBOARD_LAYOUT_H
#define LOGIN_GREETER_KEYBOARD_LAYOUT_H

#include "compositor-ipc-client.h"

/*
 * Gives the login screen the system's keyboard layout ("localectl
 * set-x11-keymap") by telling the sway instance it runs in to use it.
 * Returns the IPC connection to keep until the greeter exits, or NULL when
 * there is no layout to apply or no sway to apply it to.
 */
struct compositor_ipc_client *login_greeter_apply_system_keyboard_layout(void);

#endif
