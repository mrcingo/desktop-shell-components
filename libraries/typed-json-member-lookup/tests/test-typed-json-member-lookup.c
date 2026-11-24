#include "typed-json-member-lookup.h"

static const char
    document_with_every_type[] = "{\"text\": \"a\", \"number\": 7, \"flag\": true, \"list\": [1, \"x\"], \"child\": {}}";

static const char document_with_wrong_types[] = "{\"text\": 1, \"number\": \"7\", \"flag\": null, \"list\": {}}";

static JsonObject *
parse_object(
    JsonParser *parser,
    const char *json_text
)
{
    g_assert_true(json_parser_load_from_data(parser, json_text, -1, NULL));

    JsonObject *object = typed_json_root_object(json_parser_get_root(parser));

    g_assert_nonnull(object);

    return object;
}

static void
test_string_boolean_and_object_members_are_found(void)
{
    JsonParser *parser = json_parser_new();
    JsonObject *object = parse_object(parser, document_with_every_type);

    g_assert_cmpstr(typed_json_lookup_string_member(object, "text"), ==, "a");
    g_assert_true(typed_json_lookup_boolean_member(object, "flag", false));
    g_assert_nonnull(typed_json_lookup_object_member(object, "child"));
    g_object_unref(parser);
}

static void
test_integer_member_is_found(void)
{
    JsonParser *parser = json_parser_new();
    JsonObject *object = parse_object(parser, document_with_every_type);
    int64_t     number = 0;

    g_assert_true(typed_json_lookup_integer_member(object, "number", &number));
    g_assert_cmpint(number, ==, 7);
    g_object_unref(parser);
}

static JsonArray *
parse_example_list(
    JsonParser *parser
)
{
    JsonObject *object = parse_object(parser, document_with_every_type);

    return typed_json_lookup_array_member(object, "list");
}

static void
test_integer_element_is_found(void)
{
    JsonParser *parser  = json_parser_new();
    JsonArray  *list    = parse_example_list(parser);
    int64_t     element = 0;

    g_assert_cmpuint(typed_json_count_array_elements(list), ==, 2);
    g_assert_true(typed_json_lookup_integer_element(list, 0, &element));
    g_assert_cmpint(element, ==, 1);
    g_object_unref(parser);
}

static void
test_other_and_missing_elements_read_as_absent(void)
{
    JsonParser *parser  = json_parser_new();
    JsonArray  *list    = parse_example_list(parser);
    int64_t     element = 0;

    g_assert_false(typed_json_lookup_integer_element(list, 1, &element));
    g_assert_false(typed_json_lookup_integer_element(list, 2, &element));
    g_object_unref(parser);
}

static void
test_wrong_types_and_missing_members_read_as_absent(void)
{
    JsonParser *parser = json_parser_new();
    JsonObject *object = parse_object(parser, document_with_wrong_types);
    int64_t     number = 0;

    g_assert_null(typed_json_lookup_string_member(object, "text"));
    g_assert_null(typed_json_lookup_string_member(object, "missing"));
    g_assert_false(typed_json_lookup_integer_member(object, "number", &number));
    g_object_unref(parser);
}

static void
test_wrong_container_types_read_as_absent(void)
{
    JsonParser *parser = json_parser_new();
    JsonObject *object = parse_object(parser, document_with_wrong_types);

    g_assert_true(typed_json_lookup_boolean_member(object, "flag", true));
    g_assert_null(typed_json_lookup_array_member(object, "list"));
    g_assert_null(typed_json_lookup_object_member(object, "text"));
    g_object_unref(parser);
}

static void
test_null_inputs_read_as_empty(void)
{
    g_assert_null(typed_json_lookup_string_member(NULL, "text"));
    g_assert_cmpuint(typed_json_count_array_elements(NULL), ==, 0);
    g_assert_null(typed_json_root_object(NULL));
}

static const struct {
    const char *test_path;

    void (*test_function)(void);
} registered_tests[] = {
    {"/typed-json-member-lookup/string-boolean-object", test_string_boolean_and_object_members_are_found   },
    {"/typed-json-member-lookup/integer",               test_integer_member_is_found                       },
    {"/typed-json-member-lookup/integer-element",       test_integer_element_is_found                      },
    {"/typed-json-member-lookup/other-elements",        test_other_and_missing_elements_read_as_absent     },
    {"/typed-json-member-lookup/wrong-value-types",     test_wrong_types_and_missing_members_read_as_absent},
    {"/typed-json-member-lookup/wrong-container-types", test_wrong_container_types_read_as_absent          },
    {"/typed-json-member-lookup/null-inputs",           test_null_inputs_read_as_empty                     },
};

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);

    for (size_t i = 0; i < G_N_ELEMENTS(registered_tests); i++) {
        g_test_add_func(registered_tests[i].test_path, registered_tests[i].test_function);
    }

    return g_test_run();
}
