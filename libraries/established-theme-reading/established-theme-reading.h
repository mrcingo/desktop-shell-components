#ifndef ESTABLISHED_THEME_READING_H
#define ESTABLISHED_THEME_READING_H

#include <glib.h>

#include "compositor-bar-appearance.h"
#include "theme-palette.h"

/*
 * The established look as this project's components show it: the shipped
 * defaults, then the sway bar's colors and font, then the user's own
 * shared-theme.css, each overriding the one before.
 */

/*
 * Waits at most timeout_milliseconds for the bar, then carries on without
 * it. compositor_socket_path and user_style_sheet_path may be NULL or name
 * nothing. Never returns NULL.
 */
struct theme_palette *
established_theme_read(const char *compositor_socket_path, const char *user_style_sheet_path, guint timeout_milliseconds);

/* Applies the bar's colors and font; fields the bar does not set stay. */
void established_theme_apply_bar_appearance(struct theme_palette *palette, const struct compositor_bar_appearance *appearance);

#endif
