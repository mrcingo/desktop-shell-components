#ifndef THEME_PALETTE_H
#define THEME_PALETTE_H

#include <stdbool.h>

/*
 * The established look as plain values, for writing other programs' style
 * files: the shared theme's seven colors and the font. Starts from the
 * shipped defaults; the sway bar and the user's shared-theme.css override
 * them. Needs no toolkit.
 */

struct palette_color {
    double red; // 0 to 1
    double green;
    double blue;
    double alpha;
};

enum theme_palette_role {
    THEME_PALETTE_BACKGROUND,
    THEME_PALETTE_TEXT,
    THEME_PALETTE_SECONDARY_TEXT,
    THEME_PALETTE_ACCENT,
    THEME_PALETTE_ACCENT_TEXT,
    THEME_PALETTE_BORDER,
    THEME_PALETTE_URGENT,
    THEME_PALETTE_ROLE_COUNT,
};

struct theme_palette {
    struct palette_color colors[THEME_PALETTE_ROLE_COUNT];
    char                *font_family; // never NULL
    int                  font_size;   // points
};

/* The shipped defaults, matching sway's default bar. Free with theme_palette_destroy(). */
struct theme_palette *theme_palette_create_default(void);

void theme_palette_destroy(struct theme_palette *palette);

/* "#rgb", "#rrggbb" or "#rrggbbaa", as sway writes colors. Returns false for anything else. */
bool palette_color_parse(const char *text, struct palette_color *color);

/* "#rrggbb", or "rgba(r, g, b, a)" when not opaque. Free with g_free(). */
char *palette_color_format(struct palette_color color);

/* fraction 0 gives first, 1 gives second. */
struct palette_color palette_color_mix(struct palette_color first, struct palette_color second, double fraction);

struct palette_color palette_color_with_alpha(struct palette_color color, double alpha);

/* Relative luminance, 0 for black to 1 for white. */
double palette_color_luminance(struct palette_color color);

/* Leaves the color unchanged and returns false when text is NULL or not a color. */
bool theme_palette_set_color(struct theme_palette *palette, enum theme_palette_role role, const char *text);

/*
 * A Pango-style description such as "Monaspace Neon 10" or "pango:Monaspace
 * Neon 10": the last word, when a number, is the size. Returns false and
 * changes nothing for NULL or empty text.
 */
bool theme_palette_set_font(struct theme_palette *palette, const char *description);

/*
 * Applies "--theme-background-color: #222222;" style declarations from a
 * style sheet such as shared-theme.css. Only plain color values count;
 * references to other colors and anything unexpected are ignored.
 */
void theme_palette_apply_variables(struct theme_palette *palette, const char *style_sheet_text);

/* Whether the background is dark, so applications should prefer their dark variant. */
bool theme_palette_is_dark(const struct theme_palette *palette);

#endif
