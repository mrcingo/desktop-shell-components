#include <stdbool.h>

#include <pango/pangocairo.h>

#include "rendered-text-width-measurement.h"

struct rendered_text_width_measurement {
    PangoFontMap *font_map;
    PangoContext *context;
    PangoLayout  *layout;
};

static void
set_layout_font(
    PangoLayout *layout,
    const char  *font_description
)
{
    PangoFontDescription *font = pango_font_description_from_string(font_description);

    pango_layout_set_font_description(layout, font);
    pango_font_description_free(font);
}

/* sway draws title text with fractional glyph advances; rounded ones would measure too narrow. */
static PangoContext *
create_context_with_fractional_glyph_widths(
    PangoFontMap *font_map
)
{
    PangoContext         *context      = pango_font_map_create_context(font_map);
    cairo_font_options_t *font_options = cairo_font_options_create();

    cairo_font_options_set_hint_metrics(font_options, CAIRO_HINT_METRICS_OFF);
    pango_cairo_context_set_font_options(context, font_options);
    pango_context_set_round_glyph_positions(context, FALSE);
    cairo_font_options_destroy(font_options);

    return context;
}

struct rendered_text_width_measurement *
rendered_text_width_measurement_create(
    const char *font_description
)
{
    struct rendered_text_width_measurement *measurement = g_new0(struct rendered_text_width_measurement, 1);

    measurement->font_map = pango_cairo_font_map_new();
    measurement->context  = create_context_with_fractional_glyph_widths(measurement->font_map);
    measurement->layout   = pango_layout_new(measurement->context);

    set_layout_font(measurement->layout, font_description);

    return measurement;
}

void
rendered_text_width_measurement_destroy(
    struct rendered_text_width_measurement *measurement
)
{
    if (measurement == NULL) {
        return;
    }

    g_object_unref(measurement->layout);
    g_object_unref(measurement->context);
    g_object_unref(measurement->font_map);
    g_free(measurement);
}

static bool
is_valid_markup(
    const char *markup
)
{
    return pango_parse_markup(markup, -1, 0, NULL, NULL, NULL, NULL);
}

/* The layout is only a measuring tool; its text is replaced on every call. */
int
rendered_text_width_measurement_measure_markup(
    const struct rendered_text_width_measurement *measurement,
    const char                                   *markup
)
{
    PangoRectangle logical_extents = {0};

    if (is_valid_markup(markup)) {
        pango_layout_set_markup(measurement->layout, markup, -1);
    } else {
        pango_layout_set_text(measurement->layout, markup, -1);
    }

    pango_layout_get_extents(measurement->layout, NULL, &logical_extents);

    return PANGO_PIXELS_CEIL(logical_extents.width);
}
