#ifndef LOGIN_GREETER_SETTINGS_H
#define LOGIN_GREETER_SETTINGS_H

#include <stdbool.h>

/*
 * Settings from login-greeter.ini, which the login screen reads from
 * /etc/xdg/desktop-shell-components. Every one is optional.
 */

struct login_greeter_settings {
    char *prefilled_username;             // NULL to start with an empty field
    char *default_session_name;           // NULL for the first session
    bool  follows_system_keyboard_layout; // the layout set with localectl
};

/* override_path is the --config file, or NULL. */
struct login_greeter_settings *login_greeter_settings_load(const char *override_path);

void login_greeter_settings_destroy(struct login_greeter_settings *settings);

#endif
