#ifndef TEXT_ELLIPSIS_TRUNCATION_H
#define TEXT_ELLIPSIS_TRUNCATION_H

#include <stddef.h>

/*
 * Shortens text to at most maximum_characters characters (not bytes), ending
 * in "..." when anything was cut. Invalid UTF-8 is repaired first, so the
 * result is always valid. Free it with g_free().
 */
char *text_ellipsis_truncate(const char *text, size_t maximum_characters);

#endif
