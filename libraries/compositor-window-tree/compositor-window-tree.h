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

/* A rectangle in layout pixels, as the compositor reports "rect". */
struct compositor_rectangle {
    int x;
    int y;
    int width;
    int height;
};

/* The layout of the container a window sits in. */
enum compositor_container_layout {
    COMPOSITOR_CONTAINER_LAYOUT_UNKNOWN,
    COMPOSITOR_CONTAINER_LAYOUT_SIDE_BY_SIDE,         // splith
    COMPOSITOR_CONTAINER_LAYOUT_ON_TOP_OF_EACH_OTHER, // splitv
    COMPOSITOR_CONTAINER_LAYOUT_TABBED,
    COMPOSITOR_CONTAINER_LAYOUT_STACKED,
};

struct compositor_window {
    int64_t container_id;

    char *application_name; // app_id, X11 class or title, never NULL
    char *title;            // never NULL
    char *workspace_name;   // never NULL
    int   process_id;
    int   title_bar_width; // pixels; 0 when the window shows no title bar
    int   content_x;       // the window below its title bar, borders included, in layout pixels
    int   content_y;
    int   content_width;
    int   content_height;
    int   title_bar_height; // 0 when the window shows no title bar
    int   border_width;     // the border drawn around the content, on each side

    enum compositor_container_layout parent_layout; // UNKNOWN for floating windows

    char *toplevel_capture_identifier; // NULL if the compositor has none
    bool  has_keyboard_focus;

    bool   is_floating;
    bool   is_fullscreen;
    bool   is_sticky;
    bool   is_x11_window; // its process_id is then the X server's, shared by every X11 window
    char **marks;         // never NULL; may be empty
    bool   is_urgent;     // the window asked for attention
    bool   is_visible;    // shown now, not a hidden tab or on a hidden workspace
};

struct compositor_output {
    char *output_name; // the connector name, such as eDP-1
    int   output_x;    // top-left corner, in layout pixels
    int   output_y;

    struct compositor_rectangle output_area;
    struct compositor_rectangle workspace_area; // of the visible workspace: the output less the bars
    bool                        contains_focused_workspace;
    /* Space that panels such as a top bar reserve above the workspace. */
    int space_reserved_above_workspace;
    /* Tiled windows in layout order, then floating ones. */
    GPtrArray *windows_on_visible_workspace; // struct compositor_window
};

struct compositor_window_tree {
    GPtrArray *outputs;                   // struct compositor_output
    GPtrArray *all_windows;               // struct compositor_window on every workspace
    GArray    *window_ids_in_focus_order; // int64_t, most recent first
    /* int64_t: containers wrapped by a container that holds nothing else; see redundant-layout-wrapper-finder.h. */
    GArray *redundant_wrapper_child_ids;
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

/* A deep copy, to keep a window past the tree it came from. Free it with compositor_window_free(). */
struct compositor_window *compositor_window_copy(const struct compositor_window *window);
void                      compositor_window_free(struct compositor_window *window);

bool compositor_window_has_mark(const struct compositor_window *window, const char *mark);

/* The window with the container ID on any workspace, or NULL. */
const struct compositor_window *
compositor_window_tree_find_window(const struct compositor_window_tree *tree, int64_t container_id);

/* The window with the mark on any workspace, or NULL. */
const struct compositor_window *
compositor_window_tree_find_marked_window(const struct compositor_window_tree *tree, const char *mark);

/* The output showing the window, or NULL when its workspace is hidden. */
const struct compositor_output *
compositor_window_tree_find_output_showing_window(const struct compositor_window_tree *tree, int64_t container_id);

/* The label desktop components show for a window: "foot 4123". Free it with g_free(). */
char *compositor_window_format_name_and_process_id(const struct compositor_window *window);

#endif
