#ifndef COMPOSITOR_CONFIGURATION_DIRECTIVES_H
#define COMPOSITOR_CONFIGURATION_DIRECTIVES_H

/*
 * Reads settings from the compositor's configuration text, as its
 * GET_CONFIG reply gives it. Only top-level lines count: a "font" inside
 * "bar { ... }" belongs to the bar. Files pulled in with "include" are not
 * part of the text and are not read.
 */

/*
 * The arguments of the last top-level line whose first word is
 * directive_name, such as "pango:Monaspace Neon 10" for "font", or NULL.
 * Free the result with g_free().
 */
char *compositor_configuration_find_directive(const char *configuration_text, const char *directive_name);

#endif
