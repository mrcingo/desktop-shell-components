#ifndef WINDOW_LABEL_FORMAT_H
#define WINDOW_LABEL_FORMAT_H

#include <stdbool.h>

/*
 * Labels for windows from a format such as "{name} {pid}". Placeholders:
 *
 *   {name}       application name (app_id or X11 class)
 *   {pid}        process ID
 *   {title}      window title
 *   {workspace}  workspace name
 *
 * Unknown placeholders are kept as written, so a typo shows up on screen.
 */

struct window_label_values {
    const char *name;
    int         process_id;
    const char *title;
    const char *workspace;
};

/* Free the result with g_free(). */
char *window_label_format_expand(const char *format, const struct window_label_values *values);

/* Whether the label changes when a window's title changes. */
bool window_label_format_uses_title(const char *format);

#endif
