#include <glib.h>

#include "text-ellipsis-truncation.h"

static const char ellipsis[] = "...";

enum {
    ELLIPSIS_LENGTH = sizeof(ellipsis) - 1,
};

/* Spaces before the dots would read as a gap, so they go. */
static char *
cut_and_add_ellipsis(
    const char *valid_text,
    size_t      maximum_characters
)
{
    g_autofree char *kept_text = g_utf8_substring(valid_text, 0, (glong)(maximum_characters - ELLIPSIS_LENGTH));

    g_strchomp(kept_text);

    return g_strconcat(kept_text, ellipsis, NULL);
}

char *
text_ellipsis_truncate(
    const char *text,
    size_t      maximum_characters
)
{
    g_autofree char *valid_text = g_utf8_make_valid(text, -1);

    if ((size_t)g_utf8_strlen(valid_text, -1) <= maximum_characters) {
        return g_steal_pointer(&valid_text);
    }

    if (maximum_characters <= ELLIPSIS_LENGTH) {
        return g_strndup(ellipsis, maximum_characters);
    }

    return cut_and_add_ellipsis(valid_text, maximum_characters);
}
