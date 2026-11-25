#include <stdbool.h>

#include "redundant-layout-wrapper-finder.h"
#include "typed-json-member-lookup.h"

static const char *const child_list_names[] = {"nodes", "floating_nodes"};

/* A missing list counts as empty. */
static guint
child_list_length(
    JsonArray *children
)
{
    if (children == NULL) {
        return 0;
    }

    return json_array_get_length(children);
}

static guint
count_children(
    JsonObject *node
)
{
    guint child_count = 0;

    for (size_t i = 0; i < G_N_ELEMENTS(child_list_names); i++) {
        child_count += child_list_length(typed_json_lookup_array_member(node, child_list_names[i]));
    }

    return child_count;
}

/* NULL for an element that is not an object. */
static JsonObject *
child_object_at(
    JsonArray *children,
    guint      element_index
)
{
    JsonNode *element = json_array_get_element(children, element_index);

    if (!JSON_NODE_HOLDS_OBJECT(element)) {
        return NULL;
    }

    return json_node_get_object(element);
}

/* Windows carry a process ID; containers that only arrange others do not. */
static bool
is_arranging_container(
    JsonObject *node
)
{
    int64_t process_id = 0;

    if (g_strcmp0(typed_json_lookup_string_member(node, "type"), "con") != 0) {
        return false;
    }

    return !typed_json_lookup_integer_member(node, "pid", &process_id);
}

/* Workspaces and outputs are never removed, so only a plain container counts as a wrapper. */
static bool
is_redundant_inside(
    JsonObject *node,
    JsonObject *parent
)
{
    if (parent == NULL) {
        return false;
    }

    if (!is_arranging_container(node)) {
        return false;
    }

    if (!is_arranging_container(parent)) {
        return false;
    }

    return count_children(parent) == 1;
}

static void search_node(JsonObject *node, JsonObject *parent, GArray *found_ids);

static void
search_child_list(
    JsonObject *node,
    const char *list_name,
    GArray     *found_ids
)
{
    JsonArray *children = typed_json_lookup_array_member(node, list_name);

    for (guint i = 0; i < child_list_length(children); i++) {
        search_node(child_object_at(children, i), node, found_ids);
    }
}

static void
remember_if_redundant(
    JsonObject *node,
    JsonObject *parent,
    GArray     *found_ids
)
{
    int64_t node_id = 0;

    if (!is_redundant_inside(node, parent)) {
        return;
    }

    if (typed_json_lookup_integer_member(node, "id", &node_id)) {
        g_array_append_val(found_ids, node_id);
    }
}

/* Elements that are not objects are skipped: the tree comes from another process. */
static void
search_node(
    JsonObject *node,
    JsonObject *parent,
    GArray     *found_ids
)
{
    if (node == NULL) {
        return;
    }

    remember_if_redundant(node, parent, found_ids);

    for (size_t i = 0; i < G_N_ELEMENTS(child_list_names); i++) {
        search_child_list(node, child_list_names[i], found_ids);
    }
}

GArray *
redundant_layout_wrapper_finder_find(
    JsonNode *root_node
)
{
    GArray *found_ids = g_array_new(FALSE, FALSE, sizeof(int64_t));

    if (root_node == NULL) {
        return found_ids;
    }

    if (JSON_NODE_HOLDS_OBJECT(root_node)) {
        search_node(json_node_get_object(root_node), NULL, found_ids);
    }

    return found_ids;
}
