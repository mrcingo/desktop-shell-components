#ifndef RENDERED_TEXT_WIDTH_MEASUREMENT_H
#define RENDERED_TEXT_WIDTH_MEASUREMENT_H

/*
 * How wide text is in pixels when Pango draws it in a given font, as the
 * compositor draws title bars. Needs no window or display.
 */

struct rendered_text_width_measurement;

/* font_description as Pango reads it, such as "Monaspace Neon 10". */
struct rendered_text_width_measurement *rendered_text_width_measurement_create(const char *font_description);

void rendered_text_width_measurement_destroy(struct rendered_text_width_measurement *measurement);

/* Invalid markup is measured as the plain text it would show as. */
int
rendered_text_width_measurement_measure_markup(const struct rendered_text_width_measurement *measurement, const char *markup);

#endif
