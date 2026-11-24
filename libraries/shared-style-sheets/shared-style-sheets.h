#ifndef SHARED_STYLE_SHEETS_H
#define SHARED_STYLE_SHEETS_H

#include <gdk/gdk.h>

/*
 * Every program loads the same theme, so all desktop components share
 * colours, fonts and control shapes. Style sheets are loaded in this order,
 * each able to override the ones before:
 *
 *   1. DATA/shared-theme.css                     colours as CSS variables
 *   2. DATA/COMPONENT.css, or the override path  the program's own layout
 *   3. CONFIG/shared-theme.css                   the user's theme changes
 *   4. CONFIG/COMPONENT.css                      the user's program changes
 *
 * DATA is the installed data directory and CONFIG is
 * $XDG_CONFIG_HOME/desktop-shell-components.
 */

/*
 * The directory to read shipped style sheets from: the value of
 * $DESKTOP_SHELL_COMPONENTS_DATA_DIRECTORY when set, for running from a
 * source tree, otherwise installed_data_directory.
 */
const char *shared_style_sheets_resolve_data_directory(const char *installed_data_directory);

/* style_sheet_override_path replaces step 2 when not NULL. */
void shared_style_sheets_load_for_display(
    GdkDisplay *display,
    const char *data_directory,
    const char *component_name,
    const char *style_sheet_override_path
);

#endif
