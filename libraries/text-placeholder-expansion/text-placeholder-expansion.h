#ifndef TEXT_PLACEHOLDER_EXPANSION_H
#define TEXT_PLACEHOLDER_EXPANSION_H

#include <stddef.h>

/*
 * User-written formats such as "MEM {used}/{total}": every {name} is
 * replaced by its value. Unknown placeholders stay as written, so a typo
 * shows up on screen instead of vanishing.
 */

struct text_placeholder {
    const char *name;  // without braces
    const char *value; // NULL reads as empty
};

/* Free the result with g_free(). */
char *text_placeholder_expand(const char *format, const struct text_placeholder *placeholders, size_t placeholder_count);

#endif
