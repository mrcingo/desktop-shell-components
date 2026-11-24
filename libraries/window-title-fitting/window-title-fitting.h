#ifndef WINDOW_TITLE_FITTING_H
#define WINDOW_TITLE_FITTING_H

#include "compositor-window-tree.h"
#include "rendered-text-width-measurement.h"

/*
 * A window's title bar text (see window-title-format-command.h) with its
 * {title} shortened with "..." so that the title has at most a given number
 * of characters and the whole text fits the title bar, keeping what comes
 * after the title, such as the process ID, in view.
 */

struct window_title_fitting {
    const char *format;
    int         maximum_title_characters; // 0 for no limit
    /* NULL, or a text_space_width of 0, only applies the character limit. */
    const struct rendered_text_width_measurement *measurement;
    int                                           text_space_width; // pixels for the text
};

/* Free the result with g_free(). */
char *window_title_fitting_expand(const struct window_title_fitting *fitting, const struct compositor_window *window);

#endif
