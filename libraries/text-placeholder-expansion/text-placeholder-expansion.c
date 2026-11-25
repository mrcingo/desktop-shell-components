#include <stdbool.h>
#include <string.h>

#include <glib.h>

#include "text-placeholder-expansion.h"

struct expansion {
    GString                       *text;
    const struct text_placeholder *placeholders;
    size_t                         placeholder_count;
};

static bool
names_match(
    const char *placeholder_name,
    const char *name,
    size_t      name_length
)
{
    if (strlen(placeholder_name) != name_length) {
        return false;
    }

    return strncmp(placeholder_name, name, name_length) == 0;
}

static const struct text_placeholder *
find_placeholder(
    const struct expansion *expansion,
    const char             *name,
    size_t                  name_length
)
{
    for (size_t i = 0; i < expansion->placeholder_count; i++) {
        if (names_match(expansion->placeholders[i].name, name, name_length)) {
            return &expansion->placeholders[i];
        }
    }

    return NULL;
}

/* Appends the value for "{...}" from opening_brace to closing_brace, or the text itself when unknown. */
static void
append_placeholder(
    struct expansion *expansion,
    const char       *opening_brace,
    const char       *closing_brace
)
{
    const struct text_placeholder *placeholder = find_placeholder(
        expansion,
        opening_brace + 1,
        (size_t)(closing_brace - opening_brace - 1)
    );

    if (placeholder == NULL) {
        g_string_append_len(expansion->text, opening_brace, closing_brace + 1 - opening_brace);
        return;
    }

    if (placeholder->value != NULL) {
        g_string_append(expansion->text, placeholder->value);
    }
}

static const char *
find_closing_brace(
    const char *opening_brace
)
{
    if (opening_brace == NULL) {
        return NULL;
    }

    return strchr(opening_brace, '}');
}

/* Text without placeholders ends the format. */
static const char *
append_rest(
    struct expansion *expansion,
    const char       *remaining
)
{
    g_string_append(expansion->text, remaining);

    return NULL;
}

/* Appends up to and including the next placeholder; returns NULL once the format is used up. */
static const char *
append_next_piece(
    struct expansion *expansion,
    const char       *remaining
)
{
    const char *opening_brace = strchr(remaining, '{');
    const char *closing_brace = find_closing_brace(opening_brace);

    if (closing_brace == NULL) {
        return append_rest(expansion, remaining);
    }

    g_string_append_len(expansion->text, remaining, opening_brace - remaining);
    append_placeholder(expansion, opening_brace, closing_brace);

    return closing_brace + 1;
}

char *
text_placeholder_expand(
    const char                    *format,
    const struct text_placeholder *placeholders,
    size_t                         placeholder_count
)
{
    struct expansion expansion = {
        .text              = g_string_new(NULL),
        .placeholders      = placeholders,
        .placeholder_count = placeholder_count
    };

    const char *remaining = format;

    while (remaining != NULL) {
        remaining = append_next_piece(&expansion, remaining);
    }

    return g_string_free(expansion.text, FALSE);
}
