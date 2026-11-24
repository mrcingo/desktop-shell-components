#ifndef WINDOW_TITLE_FORMAT_COMMAND_H
#define WINDOW_TITLE_FORMAT_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "compositor-window-tree.h"

/*
 * The compositor command that sets one window's title bar text, such as
 *
 *   [con_id=7] title_format "~/Programming <span alpha='60%'>(4123)</span>"
 *
 * The format mixes the compositor's own placeholders (%title, %app_id, ...),
 * which it fills in itself, with those of window-label-format.h: {title},
 * {name}, {pid} and {workspace} are filled in here, escaped for Pango markup.
 */

/*
 * The compositor keeps backslashes inside quotes, so a format can only be
 * quoted if it lacks one of the two quote characters and does not end in
 * a backslash.
 */
bool window_title_format_can_be_quoted(const char *format);

/* The format with title_text as {title}. Free the result with g_free(). */
char *window_title_format_expand(const char *format, const struct compositor_window *window, const char *title_text);

/*
 * The command that gives the window expanded_text, which came from format.
 * Returns NULL if the format cannot be quoted. Free the result with g_free().
 */
char *window_title_format_command_build(const char *format, int64_t container_id, const char *expanded_text);

#endif
