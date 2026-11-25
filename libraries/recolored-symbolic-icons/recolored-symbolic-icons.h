#ifndef RECOLORED_SYMBOLIC_ICONS_H
#define RECOLORED_SYMBOLIC_ICONS_H

#include <stdbool.h>

#include <glib.h>

/*
 * Copies of an icon theme's single-color ("symbolic") icons drawn in the
 * theme's text color, placed in the user's own overlay of that theme
 * ($XDG_DATA_HOME/icons/Adwaita/symbolic/...). Toolkits recolor symbolic
 * icons themselves, but some programs, such as Thunar's side pane, load
 * them as plain images and show them in their fixed dark gray, which
 * disappears on a dark background.
 *
 * The overlay holds no index.theme: the theme's own index lists the same
 * folders, and the toolkit looks in the user's data directory before the
 * system's, so only these icons change. Full-color icons, such as the
 * folders in a file list, and the selected theme stay as they are.
 *
 * Only the foreground grays are replaced; status colors (green, orange,
 * red) stay.
 */

/*
 * The source icon recolored to new_color ("#rrggbb"), or NULL when it is
 * refused: larger than 64 KiB, not plain UTF-8 SVG text, referring to
 * anything outside itself (scripts, entities, links, style sheets), or
 * without any of the foreground grays. A leading XML declaration is
 * dropped, so a comment can come first. Free the result with g_free().
 */
char *recolored_symbolic_icon_recolor(const char *svg_text, gsize length, const char *new_color);

struct recolored_symbolic_icons_request {
    const char        *source_directory;  // the theme's symbolic icons by context, such as /usr/share/icons/Adwaita/symbolic
    const char        *data_directory;    // $XDG_DATA_HOME
    const char        *overlay_directory; // relative to data_directory, such as icons/Adwaita/symbolic
    const char *const *icon_names;        // without "-symbolic", such as "user-home"; NULL-terminated
    const char        *new_color;         // "#rrggbb"
    const char        *svg_marker_line;   // an XML comment
};

/*
 * Writes the copies (see managed-file-writing.h for the safety rules),
 * skipping icons whose source is missing or refused, and removes copies
 * written earlier that are no longer asked for; the user's own files in
 * the overlay are never touched. Returns false with error set when
 * something could not be written; the rest is still written.
 */
bool recolored_symbolic_icons_write(const struct recolored_symbolic_icons_request *request, GError **error);

/* Whether a name is a plain icon name: lowercase letters, digits and "-", at most 64 characters. */
bool recolored_symbolic_icon_is_plain_name(const char *icon_name);

#endif
