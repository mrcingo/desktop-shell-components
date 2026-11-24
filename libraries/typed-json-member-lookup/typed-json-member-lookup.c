#include "typed-json-member-lookup.h"

static JsonNode *
accept_value_of_type(
    JsonNode *node,
    GType     expected_type
)
{
    if (node == NULL) {
        return NULL;
    }

    if (!JSON_NODE_HOLDS_VALUE(node)) {
        return NULL;
    }

    if (json_node_get_value_type(node) != expected_type) {
        return NULL;
    }

    return node;
}

static JsonNode *
lookup_member_node(
    JsonObject *object,
    const char *member_name
)
{
    if (object == NULL) {
        return NULL;
    }

    return json_object_get_member(object, member_name);
}

const char *
typed_json_lookup_string_member(
    JsonObject *object,
    const char *member_name
)
{
    JsonNode *node = accept_value_of_type(lookup_member_node(object, member_name), G_TYPE_STRING);

    if (node == NULL) {
        return NULL;
    }

    return json_node_get_string(node);
}

bool
typed_json_lookup_integer_member(
    JsonObject *object,
    const char *member_name,
    int64_t    *integer_value
)
{
    JsonNode *node = accept_value_of_type(lookup_member_node(object, member_name), G_TYPE_INT64);

    if (node == NULL) {
        return false;
    }

    *integer_value = json_node_get_int(node);

    return true;
}

bool
typed_json_lookup_boolean_member(
    JsonObject *object,
    const char *member_name,
    bool        fallback_value
)
{
    JsonNode *node = accept_value_of_type(lookup_member_node(object, member_name), G_TYPE_BOOLEAN);

    if (node == NULL) {
        return fallback_value;
    }

    return json_node_get_boolean(node);
}

JsonArray *
typed_json_lookup_array_member(
    JsonObject *object,
    const char *member_name
)
{
    JsonNode *node = lookup_member_node(object, member_name);

    if (node == NULL) {
        return NULL;
    }

    if (!JSON_NODE_HOLDS_ARRAY(node)) {
        return NULL;
    }

    return json_node_get_array(node);
}

JsonObject *
typed_json_lookup_object_member(
    JsonObject *object,
    const char *member_name
)
{
    JsonNode *node = lookup_member_node(object, member_name);

    return typed_json_root_object(node);
}

guint
typed_json_count_array_elements(
    JsonArray *array
)
{
    if (array == NULL) {
        return 0;
    }

    return json_array_get_length(array);
}

bool
typed_json_lookup_integer_element(
    JsonArray *array,
    guint      element_index,
    int64_t   *integer_value
)
{
    if (element_index >= typed_json_count_array_elements(array)) {
        return false;
    }

    JsonNode *node = accept_value_of_type(json_array_get_element(array, element_index), G_TYPE_INT64);

    if (node == NULL) {
        return false;
    }

    *integer_value = json_node_get_int(node);

    return true;
}

JsonObject *
typed_json_root_object(
    JsonNode *root_node
)
{
    if (root_node == NULL) {
        return NULL;
    }

    if (!JSON_NODE_HOLDS_OBJECT(root_node)) {
        return NULL;
    }

    return json_node_get_object(root_node);
}
