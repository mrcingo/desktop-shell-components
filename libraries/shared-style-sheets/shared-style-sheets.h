#ifndef SHARED_STYLE_SHEETS_H
#define SHARED_STYLE_SHEETS_H

#include <gdk/gdk.h>

/*
 * The style sheets of one program, as layers that each override the ones
 * before. Nothing is forced: without any configuration the program looks
 * like the user's GTK theme, or like the user's sway bar when one is set.
 *
 *   1. DATA/shared-theme.css          theme variables taken from the GTK theme
 *   2. the compositor theme           the sway bar's colors and font, if given
 *   3. DATA/PROGRAM.css or --style    the program's layout
 *   4. /etc/xdg/desktop-shell-components/{shared-theme,PROGRAM}.css
 *   5. ~/.config/desktop-shell-components/{shared-theme,PROGRAM}.css
 *
 * DATA is the installed data directory. Missing files are simply skipped.
 */

struct shared_style_sheets;

/*
 * The directory to read shipped style sheets from: the value of
 * $DESKTOP_SHELL_COMPONENTS_DATA_DIRECTORY when set, for running from a
 * source tree, otherwise installed_data_directory.
 */
const char *shared_style_sheets_resolve_data_directory(const char *installed_data_directory);

/* Loads every layer. style_sheet_override_path replaces layer 3 when not NULL. */
struct shared_style_sheets *shared_style_sheets_create(
    GdkDisplay *display,
    const char *data_directory,
    const char *program_name,
    const char *style_sheet_override_path
);

void shared_style_sheets_destroy(struct shared_style_sheets *style_sheets);

/* Sets layer 2 from style sheet text, or clears it with NULL. */
void shared_style_sheets_set_compositor_theme(struct shared_style_sheets *style_sheets, const char *style_sheet_text);

/* Reads every file again, picking up files created, changed or removed since. */
void shared_style_sheets_reload_files(struct shared_style_sheets *style_sheets);

#endif
