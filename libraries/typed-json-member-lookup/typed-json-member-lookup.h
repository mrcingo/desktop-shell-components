#ifndef TYPED_JSON_MEMBER_LOOKUP_H
#define TYPED_JSON_MEMBER_LOOKUP_H

#include <stdbool.h>
#include <stdint.h>

#include <json-glib/json-glib.h>

/*
 * Typed lookups into JSON received from other processes. A member that is
 * missing or holds another type reads as absent instead of tripping
 * json-glib's critical warnings. A NULL object or array reads as empty.
 */

const char *typed_json_lookup_string_member(JsonObject *object, const char *member_name);
bool        typed_json_lookup_integer_member(JsonObject *object, const char *member_name, int64_t *integer_value);
bool        typed_json_lookup_boolean_member(JsonObject *object, const char *member_name, bool fallback_value);
JsonArray  *typed_json_lookup_array_member(JsonObject *object, const char *member_name);
JsonObject *typed_json_lookup_object_member(JsonObject *object, const char *member_name);

guint typed_json_count_array_elements(JsonArray *array);
bool  typed_json_lookup_integer_element(JsonArray *array, guint element_index, int64_t *integer_value);

/* The document's root object, or NULL if the root is not an object. */
JsonObject *typed_json_root_object(JsonNode *root_node);

#endif
