#include <glib.h>

#include "compositor-keyboard-layout.h"
#include "typed-json-member-lookup.h"

static const char *const layout_changes[] = {"xkb_layout", "xkb_keymap", "added", NULL};

/* NULL unless the input is a keyboard with an active layout. */
static const char *
active_layout_of(
    JsonObject *input
)
{
    if (g_strcmp0(typed_json_lookup_string_member(input, "type"), "keyboard") != 0) {
        return NULL;
    }

    return typed_json_lookup_string_member(input, "xkb_active_layout_name");
}

static JsonObject *
array_object_element(
    JsonArray *array,
    guint      element_index
)
{
    JsonNode *element = json_array_get_element(array, element_index);

    if (!JSON_NODE_HOLDS_OBJECT(element)) {
        return NULL;
    }

    return json_node_get_object(element);
}

/* NULL unless the node is an array; a NULL node is no array. */
static JsonArray *
root_array(
    JsonNode *node
)
{
    if (node == NULL) {
        return NULL;
    }

    if (!JSON_NODE_HOLDS_ARRAY(node)) {
        return NULL;
    }

    return json_node_get_array(node);
}

char *
compositor_keyboard_layout_from_inputs(
    JsonNode *inputs_reply
)
{
    JsonArray *inputs = root_array(inputs_reply);

    for (guint i = 0; i < typed_json_count_array_elements(inputs); i++) {
        const char *layout = active_layout_of(array_object_element(inputs, i));

        if (layout != NULL) {
            return g_strdup(layout);
        }
    }

    return NULL;
}

char *
compositor_keyboard_layout_from_input_event(
    JsonNode *input_event
)
{
    JsonObject *event  = typed_json_root_object(input_event);
    const char *change = typed_json_lookup_string_member(event, "change");

    if (change == NULL) {
        return NULL;
    }

    if (!g_strv_contains(layout_changes, change)) {
        return NULL;
    }

    return g_strdup(active_layout_of(typed_json_lookup_object_member(event, "input")));
}
