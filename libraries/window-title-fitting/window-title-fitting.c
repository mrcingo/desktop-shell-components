#include <stdbool.h>

#include <glib.h>

#include "text-ellipsis-truncation.h"
#include "window-title-fitting.h"
#include "window-title-format-command.h"

static char *
apply_character_limit(
    const struct window_title_fitting *fitting,
    const char                        *title
)
{
    if (fitting->maximum_title_characters <= 0) {
        return g_utf8_make_valid(title, -1);
    }

    return text_ellipsis_truncate(title, (size_t)fitting->maximum_title_characters);
}

static bool
measures_width(
    const struct window_title_fitting *fitting
)
{
    if (fitting->measurement == NULL) {
        return false;
    }

    return fitting->text_space_width > 0;
}

/* The text with the title cut to title_characters, if it fits; else NULL. */
static char *
expand_if_it_fits(
    const struct window_title_fitting *fitting,
    const struct compositor_window    *window,
    const char                        *title,
    size_t                             title_characters
)
{
    g_autofree char *shortened_title = text_ellipsis_truncate(title, title_characters);

    char *expanded_text = window_title_format_expand(fitting->format, window, shortened_title);

    if (rendered_text_width_measurement_measure_markup(fitting->measurement, expanded_text) <= fitting->text_space_width) {
        return expanded_text;
    }

    g_free(expanded_text);

    return NULL;
}

/* Bounds of a binary search over title lengths; the width grows with the length. */
struct title_length_search {
    size_t longest_fitting;  // characters known to fit, or 0
    size_t shortest_failing; // characters known not to fit
    char  *best_text;        // the text for longest_fitting, or NULL
};

static size_t
middle_length(
    const struct title_length_search *search
)
{
    return search->longest_fitting + (search->shortest_failing - search->longest_fitting) / 2;
}

/* Takes ownership of text, the text for title_characters. */
static void
record_fitting_length(
    struct title_length_search *search,
    size_t                      title_characters,
    char                       *text
)
{
    g_free(search->best_text);

    search->best_text       = text;
    search->longest_fitting = title_characters;
}

static void
try_middle_length(
    struct title_length_search        *search,
    const struct window_title_fitting *fitting,
    const struct compositor_window    *window,
    const char                        *title
)
{
    size_t middle = middle_length(search);
    char  *text   = expand_if_it_fits(fitting, window, title, middle);

    if (text == NULL) {
        search->shortest_failing = middle;
        return;
    }

    record_fitting_length(search, middle, text);
}

/* With no room even for the dots, the title goes entirely. */
static char *
expand_longest_fitting(
    const struct window_title_fitting *fitting,
    const struct compositor_window    *window,
    const char                        *title
)
{
    struct title_length_search search = {.shortest_failing = (size_t)g_utf8_strlen(title, -1)};

    while (search.longest_fitting + 1 < search.shortest_failing) {
        try_middle_length(&search, fitting, window, title);
    }

    if (search.best_text == NULL) {
        return window_title_format_expand(fitting->format, window, "");
    }

    return search.best_text;
}

char *
window_title_fitting_expand(
    const struct window_title_fitting *fitting,
    const struct compositor_window    *window
)
{
    g_autofree char *title = apply_character_limit(fitting, window->title);

    if (!measures_width(fitting)) {
        return window_title_format_expand(fitting->format, window, title);
    }

    char *whole_text = expand_if_it_fits(fitting, window, title, G_MAXSIZE);

    if (whole_text != NULL) {
        return whole_text;
    }

    return expand_longest_fitting(fitting, window, title);
}
