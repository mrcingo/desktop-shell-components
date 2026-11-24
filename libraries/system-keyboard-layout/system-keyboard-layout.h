#ifndef SYSTEM_KEYBOARD_LAYOUT_H
#define SYSTEM_KEYBOARD_LAYOUT_H

/*
 * The system's keyboard layout, as set with "localectl set-x11-keymap" and
 * kept by systemd-localed. Login screens such as GDM and SDDM use the same
 * setting, so the login screen types like the rest of the system.
 */

struct system_keyboard_layout {
    char *layout;  // such as "es"; NULL when unset
    char *model;   // NULL when unset
    char *variant; // NULL when unset
    char *options; // NULL when unset
};

/* Asks systemd-localed over the system bus. Returns NULL when it cannot be reached. */
struct system_keyboard_layout *system_keyboard_layout_read(void);

void system_keyboard_layout_destroy(struct system_keyboard_layout *layout);

/*
 * The sway command that applies the layout to every keyboard, or NULL when
 * no layout is set or a value holds characters an XKB name never has.
 * Free with g_free().
 */
char *system_keyboard_layout_to_compositor_command(const struct system_keyboard_layout *layout);

#endif
