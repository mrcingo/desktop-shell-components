#include "compositor-window-tree.h"
#include "typed-json-member-lookup.h"

static bool
is_nonempty_string(
    const char *text
)
{
    return text != NULL && text[0] != '\0';
}

/* Only windows have a process; split containers and workspaces do not. */
static bool
tree_node_is_window(
    JsonObject *tree_node
)
{
    int64_t process_id;

    return typed_json_lookup_integer_member(tree_node, "pid", &process_id);
}

static int64_t
tree_node_id(
    JsonObject *tree_node
)
{
    int64_t node_id = -1;

    typed_json_lookup_integer_member(tree_node, "id", &node_id);

    return node_id;
}

static const char *
lookup_x11_class(
    JsonObject *window_node
)
{
    JsonObject *x11_properties = typed_json_lookup_object_member(window_node, "window_properties");

    return typed_json_lookup_string_member(x11_properties, "class");
}

/* The first nonempty of the app_id, the X11 class and the title. */
/* Windows without an app_id or X11 class are named after their title. */
bool
compositor_window_name_comes_from_title(
    JsonObject *window_node
)
{
    if (is_nonempty_string(typed_json_lookup_string_member(window_node, "app_id"))) {
        return false;
    }

    return !is_nonempty_string(lookup_x11_class(window_node));
}

static char *
describe_application_name(
    JsonObject *window_node
)
{
    const char *candidate_names[] = {
        typed_json_lookup_string_member(window_node, "app_id"),
        lookup_x11_class(window_node),
        typed_json_lookup_string_member(window_node, "name"),
    };

    for (size_t i = 0; i < G_N_ELEMENTS(candidate_names); i++) {
        if (is_nonempty_string(candidate_names[i])) {
            return g_strdup(candidate_names[i]);
        }
    }

    return g_strdup("?");
}

static void
destroy_window(
    void *data
)
{
    struct compositor_window *window = data;

    g_free(window->application_name);
    g_free(window->title);
    g_free(window->workspace_name);
    g_free(window->toplevel_capture_identifier);
    g_strfreev(window->marks);
    g_free(window);
}

static void
destroy_output(
    void *data
)
{
    struct compositor_output *output = data;

    g_free(output->output_name);
    g_ptr_array_unref(output->windows_on_visible_workspace);
    g_free(output);
}

typedef void (*tree_node_visitor)(JsonObject *child_node, void *user_data);

static void
visit_array_element(
    JsonArray        *children,
    guint             element_index,
    tree_node_visitor visitor,
    void             *user_data
)
{
    JsonObject *child = typed_json_root_object(json_array_get_element(children, element_index));

    if (child == NULL) {
        return;
    }

    visitor(child, user_data);
}

static void
visit_children_in_list(
    JsonObject       *tree_node,
    const char       *child_list_name,
    tree_node_visitor visitor,
    void             *user_data
)
{
    JsonArray *children    = typed_json_lookup_array_member(tree_node, child_list_name);
    guint      child_count = typed_json_count_array_elements(children);

    for (guint i = 0; i < child_count; i++) {
        visit_array_element(children, i, visitor, user_data);
    }
}

static void
visit_child_nodes(
    JsonObject       *tree_node,
    tree_node_visitor visitor,
    void             *user_data
)
{
    static const char *const child_lists[] = {"nodes", "floating_nodes"};

    for (size_t list = 0; list < G_N_ELEMENTS(child_lists); list++) {
        visit_children_in_list(tree_node, child_lists[list], visitor, user_data);
    }
}

static int
lookup_process_id(
    JsonObject *window_node
)
{
    int64_t process_id = 0;

    typed_json_lookup_integer_member(window_node, "pid", &process_id);

    return (int)process_id;
}

static int
rectangle_left_edge(
    JsonObject *tree_node
)
{
    JsonObject *rectangle = typed_json_lookup_object_member(tree_node, "rect");
    int64_t     left_edge = 0;

    typed_json_lookup_integer_member(rectangle, "x", &left_edge);

    return (int)left_edge;
}

static int
rectangle_top_edge(
    JsonObject *tree_node
)
{
    JsonObject *rectangle = typed_json_lookup_object_member(tree_node, "rect");
    int64_t     top_edge  = 0;

    typed_json_lookup_integer_member(rectangle, "y", &top_edge);

    return (int)top_edge;
}

/* sway's "deco_rect" is the title bar, already narrowed to its tab in tabbed layouts. */
/* Missing or invalid members leave the rectangle empty. */
static struct compositor_rectangle
read_rectangle(
    JsonObject *tree_node,
    const char *member_name
)
{
    JsonObject *rectangle = typed_json_lookup_object_member(tree_node, member_name);
    int64_t     values[4] = {0};

    typed_json_lookup_integer_member(rectangle, "x", &values[0]);
    typed_json_lookup_integer_member(rectangle, "y", &values[1]);
    typed_json_lookup_integer_member(rectangle, "width", &values[2]);
    typed_json_lookup_integer_member(rectangle, "height", &values[3]);

    return (
        struct compositor_rectangle
    ){(int)values[0], (int)values[1], (int)CLAMP(values[2], 0, G_MAXINT), (int)CLAMP(values[3], 0, G_MAXINT)};
}

static int
read_border_width(
    JsonObject *window_node
)
{
    int64_t border_width = 0;

    typed_json_lookup_integer_member(window_node, "current_border_width", &border_width);

    return (int)CLAMP(border_width, 0, G_MAXINT);
}

static const struct {
    const char                      *name;
    enum compositor_container_layout layout;
} container_layouts[] = {
    {"splith",  COMPOSITOR_CONTAINER_LAYOUT_SIDE_BY_SIDE        },
    {"splitv",  COMPOSITOR_CONTAINER_LAYOUT_ON_TOP_OF_EACH_OTHER},
    {"tabbed",  COMPOSITOR_CONTAINER_LAYOUT_TABBED              },
    {"stacked", COMPOSITOR_CONTAINER_LAYOUT_STACKED             },
};

static enum compositor_container_layout
read_container_layout(
    JsonObject *container_node
)
{
    const char *layout_name = typed_json_lookup_string_member(container_node, "layout");

    for (size_t i = 0; i < G_N_ELEMENTS(container_layouts); i++) {
        if (g_strcmp0(layout_name, container_layouts[i].name) == 0) {
            return container_layouts[i].layout;
        }
    }

    return COMPOSITOR_CONTAINER_LAYOUT_UNKNOWN;
}

/* Floating windows sit in no layout. */
static enum compositor_container_layout
read_parent_layout(
    const struct compositor_window *window,
    JsonObject                     *parent_node
)
{
    if (window->is_floating) {
        return COMPOSITOR_CONTAINER_LAYOUT_UNKNOWN;
    }

    return read_container_layout(parent_node);
}

static int
lookup_title_bar_width(
    JsonObject *window_node
)
{
    JsonObject *title_bar_rectangle = typed_json_lookup_object_member(window_node, "deco_rect");
    int64_t     width               = 0;

    typed_json_lookup_integer_member(title_bar_rectangle, "width", &width);

    return (int)CLAMP(width, 0, G_MAXINT);
}

static char *
copy_string_member_or_empty(
    JsonObject *node,
    const char *member_name
)
{
    const char *value = typed_json_lookup_string_member(node, member_name);

    if (value == NULL) {
        return g_strdup("");
    }

    return g_strdup(value);
}

static void
append_string_element(
    GPtrArray *strings,
    JsonArray *array,
    guint      element_index
)
{
    JsonNode *element = json_array_get_element(array, element_index);

    if (!JSON_NODE_HOLDS_VALUE(element)) {
        return;
    }

    if (json_node_get_value_type(element) != G_TYPE_STRING) {
        return;
    }

    g_ptr_array_add(strings, g_strdup(json_node_get_string(element)));
}

/* array may be NULL. */
static void
append_string_elements(
    GPtrArray *strings,
    JsonArray *array
)
{
    if (array == NULL) {
        return;
    }

    for (guint i = 0; i < json_array_get_length(array); i++) {
        append_string_element(strings, array, i);
    }
}

static bool
lookup_fullscreen_state(
    JsonObject *window_node
)
{
    int64_t fullscreen_mode = 0;

    typed_json_lookup_integer_member(window_node, "fullscreen_mode", &fullscreen_mode);

    return fullscreen_mode != 0;
}

/* Elements that are not strings are skipped. */
static char **
copy_marks(
    JsonObject *window_node
)
{
    GPtrArray *marks = g_ptr_array_new();

    append_string_elements(marks, typed_json_lookup_array_member(window_node, "marks"));
    g_ptr_array_add(marks, NULL);

    return (char **)g_ptr_array_free(marks, FALSE);
}

/* Sizes and layout, which only components that place windows need. */
static void
read_window_geometry(
    struct compositor_window *window,
    JsonObject               *window_node,
    JsonObject               *parent_node
)
{
    struct compositor_rectangle content = read_rectangle(window_node, "rect");

    window->content_width    = content.width;
    window->content_height   = content.height;
    window->title_bar_height = read_rectangle(window_node, "deco_rect").height;
    window->border_width     = read_border_width(window_node);
    window->parent_layout    = read_parent_layout(window, parent_node);
}

static struct compositor_window *
parse_window(
    JsonObject *window_node,
    JsonObject *parent_node,
    const char *workspace_name
)
{
    struct compositor_window *window = g_new0(struct compositor_window, 1);

    *window = (struct compositor_window){
        .container_id                = tree_node_id(window_node),
        .application_name            = describe_application_name(window_node),
        .title                       = copy_string_member_or_empty(window_node, "name"),
        .workspace_name              = g_strdup(workspace_name),
        .process_id                  = lookup_process_id(window_node),
        .title_bar_width             = lookup_title_bar_width(window_node),
        .content_x                   = rectangle_left_edge(window_node),
        .content_y                   = rectangle_top_edge(window_node),
        .toplevel_capture_identifier = g_strdup(typed_json_lookup_string_member(window_node, "foreign_toplevel_identifier")),
        .has_keyboard_focus          = typed_json_lookup_boolean_member(window_node, "focused", false),
        .is_floating                 = g_strcmp0(typed_json_lookup_string_member(window_node, "type"), "floating_con") == 0,
        .is_fullscreen               = lookup_fullscreen_state(window_node),
        .is_sticky                   = typed_json_lookup_boolean_member(window_node, "sticky", false),
        .is_x11_window               = g_strcmp0(typed_json_lookup_string_member(window_node, "shell"), "xwayland") == 0,
        .marks                       = copy_marks(window_node),
        .is_urgent                   = typed_json_lookup_boolean_member(window_node, "urgent", false),
        .is_visible                  = typed_json_lookup_boolean_member(window_node, "visible", false),
    };

    read_window_geometry(window, window_node, parent_node);

    return window;
}

struct window_collection {
    GPtrArray  *windows;
    const char *workspace_name;
    JsonObject *parent_node; // the container whose children are being visited
};

static void collect_windows_in_layout_order(JsonObject *tree_node, void *user_data);

/* Children of a split, tabbed or stacked container, which is then their parent. */
static void
collect_windows_of_container(
    JsonObject                     *container_node,
    const struct window_collection *outer_collection
)
{
    struct window_collection collection = *outer_collection;

    collection.parent_node = container_node;

    visit_child_nodes(container_node, collect_windows_in_layout_order, &collection);
}

static void
collect_windows_in_layout_order(
    JsonObject *tree_node,
    void       *user_data
)
{
    struct window_collection *collection = user_data;

    if (!tree_node_is_window(tree_node)) {
        collect_windows_of_container(tree_node, collection);
        return;
    }

    g_ptr_array_add(collection->windows, parse_window(tree_node, collection->parent_node, collection->workspace_name));
}

/* Tiled windows in layout order, then floating ones. */
static void
collect_workspace_windows(
    JsonObject *workspace_node,
    GPtrArray  *windows
)
{
    struct window_collection collection = {
        .windows        = windows,
        .workspace_name = typed_json_lookup_string_member(workspace_node, "name"),
        .parent_node    = workspace_node,
    };

    visit_child_nodes(workspace_node, collect_windows_in_layout_order, &collection);
}

struct child_search {
    int64_t     wanted_id;
    JsonObject *found_child;
};

static void
match_child_by_id(
    JsonObject *child_node,
    void       *user_data
)
{
    struct child_search *search = user_data;

    if (search->found_child != NULL) {
        return;
    }

    if (tree_node_id(child_node) == search->wanted_id) {
        search->found_child = child_node;
    }
}

static JsonObject *
find_child_by_id(
    JsonObject *tree_node,
    int64_t     wanted_id
)
{
    struct child_search search = {.wanted_id = wanted_id};

    visit_child_nodes(tree_node, match_child_by_id, &search);

    return search.found_child;
}

/* Every container lists its children's ids under "focus", most recent first. */
static JsonObject *
find_child_in_focus_position(
    JsonObject *tree_node,
    guint       focus_position
)
{
    JsonArray *focus_order = typed_json_lookup_array_member(tree_node, "focus");
    int64_t    child_id;

    if (!typed_json_lookup_integer_element(focus_order, focus_position, &child_id)) {
        return NULL;
    }

    return find_child_by_id(tree_node, child_id);
}

static void
collect_window_ids_in_focus_order(
    JsonObject *tree_node,
    GArray     *window_ids
)
{
    if (tree_node_is_window(tree_node)) {
        int64_t window_id = tree_node_id(tree_node);

        g_array_append_val(window_ids, window_id);

        return;
    }

    JsonObject *child;

    for (guint position = 0; (child = find_child_in_focus_position(tree_node, position)) != NULL; position++) {
        collect_window_ids_in_focus_order(child, window_ids);
    }
}

/* The scratchpad lives on a hidden output named __i3. */
static bool
is_hidden_output_name(
    const char *output_name
)
{
    return g_str_has_prefix(output_name, "__");
}

static void
read_output_position(
    struct compositor_output *output,
    JsonObject               *output_node
)
{
    output->output_x    = rectangle_left_edge(output_node);
    output->output_y    = rectangle_top_edge(output_node);
    output->output_area = read_rectangle(output_node, "rect");
}

static struct compositor_output *
create_output(
    JsonObject *output_node,
    JsonObject *focused_output_node
)
{
    struct compositor_output *output = g_new0(struct compositor_output, 1);

    output->output_name                  = g_strdup(typed_json_lookup_string_member(output_node, "name"));
    output->contains_focused_workspace   = output_node == focused_output_node;
    output->windows_on_visible_workspace = g_ptr_array_new_with_free_func(destroy_window);

    read_output_position(output, output_node);

    return output;
}

/* The most recently focused workspace is the one shown. */
static void
collect_visible_workspace(
    struct compositor_output *output,
    JsonObject               *output_node
)
{
    JsonObject *visible_workspace = find_child_in_focus_position(output_node, 0);

    if (visible_workspace == NULL) {
        return;
    }

    output->space_reserved_above_workspace = rectangle_top_edge(visible_workspace) - rectangle_top_edge(output_node);
    output->workspace_area                 = read_rectangle(visible_workspace, "rect");

    collect_workspace_windows(visible_workspace, output->windows_on_visible_workspace);
}

static struct compositor_output *
parse_output(
    JsonObject *output_node,
    JsonObject *focused_output_node
)
{
    struct compositor_output *output = create_output(output_node, focused_output_node);

    collect_visible_workspace(output, output_node);

    return output;
}

struct output_collection {
    GPtrArray *outputs;

    JsonObject *focused_output_node; // NULL if no output has focus
};

static void
collect_visible_output(
    JsonObject *output_node,
    void       *user_data
)
{
    struct output_collection *collection  = user_data;
    const char               *output_name = typed_json_lookup_string_member(output_node, "name");

    if (output_name == NULL) {
        return;
    }

    if (is_hidden_output_name(output_name)) {
        return;
    }

    g_ptr_array_add(collection->outputs, parse_output(output_node, collection->focused_output_node));
}

static void
collect_all_windows_of_workspace(
    JsonObject *workspace_node,
    void       *user_data
)
{
    collect_workspace_windows(workspace_node, user_data);
}

/* The scratchpad's hidden output holds no window a user can see. */
static void
collect_all_windows_of_output(
    JsonObject *output_node,
    void       *user_data
)
{
    const char *output_name = typed_json_lookup_string_member(output_node, "name");

    if (output_name == NULL) {
        return;
    }

    if (is_hidden_output_name(output_name)) {
        return;
    }

    visit_child_nodes(output_node, collect_all_windows_of_workspace, user_data);
}

static bool
is_root_node(
    JsonObject *root
)
{
    if (root == NULL) {
        return false;
    }

    const char *node_type = typed_json_lookup_string_member(root, "type");

    if (g_strcmp0(node_type, "root") != 0) {
        return false;
    }

    return typed_json_lookup_array_member(root, "nodes") != NULL;
}

static struct compositor_window_tree *
create_empty_tree(void)
{
    struct compositor_window_tree *tree = g_new0(struct compositor_window_tree, 1);

    tree->outputs                   = g_ptr_array_new_with_free_func(destroy_output);
    tree->all_windows               = g_ptr_array_new_with_free_func(destroy_window);
    tree->window_ids_in_focus_order = g_array_new(FALSE, FALSE, sizeof(int64_t));

    return tree;
}

static void
collect_visible_outputs(
    JsonObject *root,
    GPtrArray  *outputs
)
{
    struct output_collection collection = {
        .outputs             = outputs,
        .focused_output_node = find_child_in_focus_position(root, 0),
    };

    visit_child_nodes(root, collect_visible_output, &collection);
}

static void
fill_tree(
    struct compositor_window_tree *tree,
    JsonObject                    *root
)
{
    collect_visible_outputs(root, tree->outputs);
    visit_child_nodes(root, collect_all_windows_of_output, tree->all_windows);
    collect_window_ids_in_focus_order(root, tree->window_ids_in_focus_order);
}

struct compositor_window_tree *
compositor_window_tree_parse(
    JsonNode *root_node
)
{
    JsonObject *root = typed_json_root_object(root_node);

    if (!is_root_node(root)) {
        return NULL;
    }

    struct compositor_window_tree *tree = create_empty_tree();

    fill_tree(tree, root);

    return tree;
}

void
compositor_window_tree_destroy(
    struct compositor_window_tree *tree
)
{
    if (tree == NULL) {
        return;
    }

    g_ptr_array_unref(tree->outputs);
    g_ptr_array_unref(tree->all_windows);
    g_array_unref(tree->window_ids_in_focus_order);
    g_free(tree);
}

const struct compositor_output *
compositor_window_tree_find_output_by_name(
    const struct compositor_window_tree *tree,
    const char                          *output_name
)
{
    if (tree == NULL) {
        return NULL;
    }

    for (guint i = 0; i < tree->outputs->len; i++) {
        const struct compositor_output *output = g_ptr_array_index(tree->outputs, i);

        if (g_strcmp0(output->output_name, output_name) == 0) {
            return output;
        }
    }

    return NULL;
}

const struct compositor_output *
compositor_window_tree_find_focused_output(
    const struct compositor_window_tree *tree
)
{
    if (tree == NULL) {
        return NULL;
    }

    for (guint i = 0; i < tree->outputs->len; i++) {
        const struct compositor_output *output = g_ptr_array_index(tree->outputs, i);

        if (output->contains_focused_workspace) {
            return output;
        }
    }

    return NULL;
}

char *
compositor_window_format_name_and_process_id(
    const struct compositor_window *window
)
{
    return g_strdup_printf("%s %d", window->application_name, window->process_id);
}

bool
compositor_window_has_mark(
    const struct compositor_window *window,
    const char                     *mark
)
{
    return g_strv_contains((const char *const *)window->marks, mark);
}

const struct compositor_window *
compositor_window_tree_find_marked_window(
    const struct compositor_window_tree *tree,
    const char                          *mark
)
{
    for (guint i = 0; i < tree->all_windows->len; i++) {
        const struct compositor_window *window = g_ptr_array_index(tree->all_windows, i);

        if (compositor_window_has_mark(window, mark)) {
            return window;
        }
    }

    return NULL;
}

static bool
output_shows_window(
    const struct compositor_output *output,
    int64_t                         container_id
)
{
    for (guint i = 0; i < output->windows_on_visible_workspace->len; i++) {
        const struct compositor_window *window = g_ptr_array_index(output->windows_on_visible_workspace, i);

        if (window->container_id == container_id) {
            return true;
        }
    }

    return false;
}

const struct compositor_output *
compositor_window_tree_find_output_showing_window(
    const struct compositor_window_tree *tree,
    int64_t                              container_id
)
{
    for (guint i = 0; i < tree->outputs->len; i++) {
        const struct compositor_output *output = g_ptr_array_index(tree->outputs, i);

        if (output_shows_window(output, container_id)) {
            return output;
        }
    }

    return NULL;
}

const struct compositor_window *
compositor_window_tree_find_window(
    const struct compositor_window_tree *tree,
    int64_t                              container_id
)
{
    for (guint i = 0; i < tree->all_windows->len; i++) {
        const struct compositor_window *window = g_ptr_array_index(tree->all_windows, i);

        if (window->container_id == container_id) {
            return window;
        }
    }

    return NULL;
}

struct compositor_window *
compositor_window_copy(
    const struct compositor_window *window
)
{
    struct compositor_window *copy = g_memdup2(window, sizeof(*window));

    copy->application_name            = g_strdup(window->application_name);
    copy->title                       = g_strdup(window->title);
    copy->workspace_name              = g_strdup(window->workspace_name);
    copy->toplevel_capture_identifier = g_strdup(window->toplevel_capture_identifier);
    copy->marks                       = g_strdupv(window->marks);

    return copy;
}

void
compositor_window_free(
    struct compositor_window *window
)
{
    if (window == NULL) {
        return;
    }

    destroy_window(window);
}
