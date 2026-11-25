#ifndef TOOLKIT_STYLE_SHEET_WRITER_H
#define TOOLKIT_STYLE_SHEET_WRITER_H

#include "theme-palette.h"

/*
 * Per-user style sheets that give other programs the established look.
 *
 * The GTK 3 sheet restyles whole widgets: GTK 3's own theme uses literal
 * colors, so named colors alone change little. The GTK 4 sheet only
 * defines named colors and libadwaita's color variables: a user style
 * sheet outranks every program's own, so widget rules there would also
 * restyle this project's own GTK 4 components.
 *
 * Both start with no marker; the caller adds its own first line.
 */

/* Free the results with g_free(). */
char *toolkit_style_sheet_write_version_3(const struct theme_palette *palette);
char *toolkit_style_sheet_write_version_4(const struct theme_palette *palette);

/* The font family with only letters, digits, spaces, '-', '_' and '.', so it cannot break out of a quoted string. */
char *toolkit_style_sheet_safe_font_family(const char *font_family);

#endif
