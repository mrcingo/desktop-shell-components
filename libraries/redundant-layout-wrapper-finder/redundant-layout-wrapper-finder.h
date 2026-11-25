#ifndef REDUNDANT_LAYOUT_WRAPPER_FINDER_H
#define REDUNDANT_LAYOUT_WRAPPER_FINDER_H

#include <glib.h>
#include <json-glib/json-glib.h>

/*
 * Finds useless nesting in the compositor's layout tree: a container whose
 * only child is another container, such as a tab group holding nothing but
 * another tab group. sway shows the inner one as an extra title bar, like
 * "T[foot thunar]". Running "split none" on the inner container removes
 * the wrapper around it and keeps its windows and layout.
 */

/* The inner containers' IDs (int64_t), in tree order; never NULL. Free with g_array_unref(). */
GArray *redundant_layout_wrapper_finder_find(JsonNode *root_node);

#endif
