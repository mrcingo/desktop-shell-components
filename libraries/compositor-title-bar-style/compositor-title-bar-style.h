#ifndef COMPOSITOR_TITLE_BAR_STYLE_H
#define COMPOSITOR_TITLE_BAR_STYLE_H

/*
 * How the compositor draws title bars, read from its configuration text
 * (see compositor-configuration-directives.h): the font and the space kept
 * free around the text. Settings it does not set keep sway's defaults.
 */

struct compositor_title_bar_style {
    char *font_description;   // for Pango, such as "Monaspace Neon 10"; never NULL
    int   horizontal_padding; // titlebar_padding, on each side
    int   border_thickness;   // titlebar_border_thickness, on each side
};

struct compositor_title_bar_style *compositor_title_bar_style_parse(const char *configuration_text);

void compositor_title_bar_style_destroy(struct compositor_title_bar_style *style);

/* Pixels left for text in a title bar title_bar_width pixels wide; never negative. */
int compositor_title_bar_style_text_space(const struct compositor_title_bar_style *style, int title_bar_width);

#endif
