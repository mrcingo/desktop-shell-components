#include <string.h>

#include <glib.h>

#include "window-label-format.h"

static char *
copy_text_or_empty(
    const char *text
)
{
    if (text == NULL) {
        return g_strdup("");
    }

    return g_strdup(text);
}

static char *
expand_name(
    const struct window_label_values *values
)
{
    return copy_text_or_empty(values->name);
}

static char *
expand_process_id(
    const struct window_label_values *values
)
{
    return g_strdup_printf("%d", values->process_id);
}

static char *
expand_title(
    const struct window_label_values *values
)
{
    return copy_text_or_empty(values->title);
}

static char *
expand_workspace(
    const struct window_label_values *values
)
{
    return copy_text_or_empty(values->workspace);
}

static const struct {
    const char *name;
    char *(*expand)(const struct window_label_values *values);
} placeholders[] = {
    {"name",      expand_name      },
    {"pid",       expand_process_id},
    {"title",     expand_title     },
    {"workspace", expand_workspace },
};

/* NULL for an unknown placeholder. */
static char *
placeholder_value(
    const char                       *placeholder,
    const struct window_label_values *values
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(placeholders); i++) {
        if (strcmp(placeholder, placeholders[i].name) == 0) {
            return placeholders[i].expand(values);
        }
    }

    return NULL;
}

/* An unknown placeholder is kept as written. */
static void
append_placeholder_value(
    GString                          *label,
    const char                       *opening_brace,
    const char                       *closing_brace,
    const struct window_label_values *values
)
{
    g_autofree char *placeholder = g_strndup(opening_brace + 1, (gsize)(closing_brace - opening_brace - 1));
    g_autofree char *value       = placeholder_value(placeholder, values);

    if (value == NULL) {
        g_string_append_len(label, opening_brace, closing_brace + 1 - opening_brace);
        return;
    }

    g_string_append(label, value);
}

/* Appends one placeholder starting at "{" and returns the text after it. */
static const char *
append_placeholder(
    GString                          *label,
    const char                       *opening_brace,
    const struct window_label_values *values
)
{
    const char *closing_brace = strchr(opening_brace, '}');

    if (closing_brace == NULL) {
        g_string_append(label, opening_brace);
        return opening_brace + strlen(opening_brace);
    }

    append_placeholder_value(label, opening_brace, closing_brace, values);

    return closing_brace + 1;
}

/* Appends text up to and including the next placeholder; returns NULL once the format is used up. */
static const char *
append_next_piece(
    GString                          *label,
    const char                       *remaining,
    const struct window_label_values *values
)
{
    const char *opening_brace = strchr(remaining, '{');

    if (opening_brace == NULL) {
        g_string_append(label, remaining);
        return NULL;
    }

    g_string_append_len(label, remaining, opening_brace - remaining);

    return append_placeholder(label, opening_brace, values);
}

char *
window_label_format_expand(
    const char                       *format,
    const struct window_label_values *values
)
{
    GString    *label     = g_string_new(NULL);
    const char *remaining = format;

    while (remaining != NULL) {
        remaining = append_next_piece(label, remaining, values);
    }

    return g_string_free(label, FALSE);
}

bool
window_label_format_uses_title(
    const char *format
)
{
    return strstr(format, "{title}") != NULL;
}
