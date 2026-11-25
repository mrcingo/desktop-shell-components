#ifndef DESKTOP_INTERFACE_SETTINGS_H
#define DESKTOP_INTERFACE_SETTINGS_H

#include <stdbool.h>

/*
 * The desktop-wide interface settings many applications follow through
 * GSettings (org.gnome.desktop.interface): the color scheme, which
 * libadwaita, the portals and Chromium-based programs read, and the font.
 * Does nothing when that schema is not installed; changes only values that
 * differ, so an unchanged theme costs no writes.
 */

/* Returns false when the schema or its keys are missing. */
bool desktop_interface_settings_apply(bool prefers_dark, const char *font_name);

#endif
