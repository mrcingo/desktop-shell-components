#ifndef COMPOSITOR_WINDOW_TREE_H
#define COMPOSITOR_WINDOW_TREE_H

#include <stdbool.h>
#include <stdint.h>

#include <glib.h>
#include <json-glib/json-glib.h>

/*
 * The parts of the compositor's layout tree (the GET_WINDOW_TREE reply) that
 * desktop components need: per output, the windows of the visible workspace,
 * and the order in which windows were last focused.
 */

struct compositor_window {
    int64_t container_id;

    char *application_name; // app_id, X11 class or title, never NULL
    char *title;            // never NULL
    char *workspace_name;   // never NULL
    int   process_id;
    int   title_bar_width; // pixels; 0 when the window shows no title bar

    char *toplevel_capture_identifier; // NULL if the compositor has none
    bool  has_keyboard_focus;
    bool  is_urgent; // the window asked for attention
};

struct compositor_output {
    char *output_name; // the connector name, such as eDP-1
    bool  contains_focused_workspace;
    /* Space that panels such as a top bar reserve above the workspace. */
    int space_reserved_above_workspace;
    /* Tiled windows in layout order, then floating ones. */
    GPtrArray *windows_on_visible_workspace; // struct compositor_window
};

struct compositor_window_tree {
    GPtrArray *outputs;                   // struct compositor_output
    GPtrArray *all_windows;               // struct compositor_window on every workspace
    GArray    *window_ids_in_focus_order; // int64_t, most recent first
};

/* Returns NULL if root_node is not a window tree. */
struct compositor_window_tree *compositor_window_tree_parse(JsonNode *root_node);
void                           compositor_window_tree_destroy(struct compositor_window_tree *tree);

const struct compositor_output *
compositor_window_tree_find_output_by_name(const struct compositor_window_tree *tree, const char *output_name);
const struct compositor_output *compositor_window_tree_find_focused_output(const struct compositor_window_tree *tree);

/*
 * Whether the window node, as found in the tree or in a window event's
 * "container", is named after its title, so a title change renames it.
 */
bool compositor_window_name_comes_from_title(JsonObject *window_node);

/* The label desktop components show for a window: "foot 4123". Free it with g_free(). */
char *compositor_window_format_name_and_process_id(const struct compositor_window *window);

#endif
