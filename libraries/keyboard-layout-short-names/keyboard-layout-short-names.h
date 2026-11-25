#ifndef KEYBOARD_LAYOUT_SHORT_NAMES_H
#define KEYBOARD_LAYOUT_SHORT_NAMES_H

/*
 * Short names for keyboard layouts ("es" for "Spanish", also for its
 * variants such as "Spanish (Dvorak)"), from the XKB rules file that lists
 * every layout, normally /usr/share/X11/xkb/rules/evdev.xml.
 */

/* NULL when the file cannot be read or does not list the layout. Free the result with g_free(). */
char *keyboard_layout_short_name_find(const char *rules_file_path, const char *layout_description);

#endif
