#include <glib.h>

#include "compositor-configuration-directives.h"
#include "compositor-title-bar-style.h"

/* sway's defaults, from sway(5). */
static const char default_font_description[] = "monospace 10";

enum {
    DEFAULT_HORIZONTAL_PADDING = 5,
    DEFAULT_BORDER_THICKNESS   = 1,
    MAXIMUM_SPACING            = 1000,
};

/* sway accepts "pango:Name 10" and "\"Name\" 10"; Pango wants "Name 10". */
static char *
read_font_description(
    const char *configuration_text
)
{
    g_autofree char *arguments = compositor_configuration_find_directive(configuration_text, "font");

    if (arguments == NULL) {
        return g_strdup(default_font_description);
    }

    const char *without_prefix = g_str_has_prefix(arguments, "pango:") ? arguments + 6 : arguments;

    g_auto(GStrv) quoted_parts = g_strsplit(without_prefix, "\"", -1);

    return g_strstrip(g_strjoinv("", quoted_parts));
}

/* The first number after the directive; "titlebar_padding 7 2" sets the horizontal padding to 7. */
static int
read_first_number(
    const char *configuration_text,
    const char *directive_name,
    int         default_value
)
{
    g_autofree char *arguments = compositor_configuration_find_directive(configuration_text, directive_name);

    gint64 number = 0;

    if (arguments == NULL) {
        return default_value;
    }

    if (!g_ascii_string_to_signed(g_strdelimit(arguments, " \t", '\0'), 10, 0, MAXIMUM_SPACING, &number, NULL)) {
        return default_value;
    }

    return (int)number;
}

struct compositor_title_bar_style *
compositor_title_bar_style_parse(
    const char *configuration_text
)
{
    struct compositor_title_bar_style *style = g_new0(struct compositor_title_bar_style, 1);

    style->font_description   = read_font_description(configuration_text);
    style->horizontal_padding = read_first_number(configuration_text, "titlebar_padding", DEFAULT_HORIZONTAL_PADDING);
    style->border_thickness   = read_first_number(configuration_text, "titlebar_border_thickness", DEFAULT_BORDER_THICKNESS);

    return style;
}

void
compositor_title_bar_style_destroy(
    struct compositor_title_bar_style *style
)
{
    if (style == NULL) {
        return;
    }

    g_free(style->font_description);
    g_free(style);
}

int
compositor_title_bar_style_text_space(
    const struct compositor_title_bar_style *style,
    int                                      title_bar_width
)
{
    int side_space = style->horizontal_padding + style->border_thickness;

    return MAX(0, title_bar_width - 2 * side_space);
}
