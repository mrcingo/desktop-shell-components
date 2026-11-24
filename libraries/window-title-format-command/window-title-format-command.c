#include <inttypes.h>
#include <string.h>

#include <glib.h>

#include "window-label-format.h"
#include "window-title-format-command.h"

/* The quote character the format does not contain, or 0 if it contains both. */
static char
choose_quote_character(
    const char *format
)
{
    if (strchr(format, '"') == NULL) {
        return '"';
    }

    if (strchr(format, '\'') == NULL) {
        return '\'';
    }

    return 0;
}

static bool
ends_with_backslash(
    const char *format
)
{
    size_t length = strlen(format);

    if (length == 0) {
        return false;
    }

    return format[length - 1] == '\\';
}

bool
window_title_format_can_be_quoted(
    const char *format
)
{
    if (ends_with_backslash(format)) {
        return false;
    }

    return choose_quote_character(format) != 0;
}

/* Window values become markup-safe text; quotes turn into entities, so they never end the command's quotes. */
char *
window_title_format_expand(
    const char                     *format,
    const struct compositor_window *window,
    const char                     *title_text
)
{
    g_autofree char *escaped_title     = g_markup_escape_text(title_text, -1);
    g_autofree char *escaped_name      = g_markup_escape_text(window->application_name, -1);
    g_autofree char *escaped_workspace = g_markup_escape_text(window->workspace_name, -1);

    const struct window_label_values values = {
        .name       = escaped_name,
        .process_id = window->process_id,
        .title      = escaped_title,
        .workspace  = escaped_workspace,
    };

    return window_label_format_expand(format, &values);
}

char *
window_title_format_command_build(
    const char *format,
    int64_t     container_id,
    const char *expanded_text
)
{
    if (!window_title_format_can_be_quoted(format)) {
        return NULL;
    }

    char quote_character = choose_quote_character(format);

    return g_strdup_printf(
        "[con_id=%" PRId64 "] title_format %c%s%c",
        container_id,
        quote_character,
        expanded_text,
        quote_character
    );
}
