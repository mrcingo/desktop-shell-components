#include "redundant-layout-wrapper-finder.h"

/*
 * Workspace 1 holds a tab group whose only child is another tab group with
 * two windows, as left behind by layout changes; workspace 2 holds a tab
 * group with two windows and a container with a single window, both fine.
 */
static const char example_tree[] = "{\"id\": 1, \"type\": \"root\", \"nodes\": [{\"id\": 2, \"type\": \"output\", \"nodes\": ["
                                   "{\"id\": 3, \"type\": \"workspace\", \"nodes\": ["
                                   "  {\"id\": 8, \"type\": \"con\", \"layout\": \"tabbed\", \"nodes\": ["
                                   "    {\"id\": 7, \"type\": \"con\", \"layout\": \"tabbed\", \"nodes\": ["
                                   "      {\"id\": 5, \"type\": \"con\", \"pid\": 10, \"nodes\": []},"
                                   "      {\"id\": 6, \"type\": \"con\", \"pid\": 11, \"nodes\": []}]}]}]},"
                                   "{\"id\": 4, \"type\": \"workspace\", \"nodes\": ["
                                   "  {\"id\": 9, \"type\": \"con\", \"layout\": \"tabbed\", \"nodes\": ["
                                   "    {\"id\": 12, \"type\": \"con\", \"pid\": 12, \"nodes\": []},"
                                   "    {\"id\": 13, \"type\": \"con\", \"pid\": 13, \"nodes\": []}]},"
                                   "  {\"id\": 14, \"type\": \"con\", \"layout\": \"splitv\", \"nodes\": ["
                                   "    {\"id\": 15, \"type\": \"con\", \"pid\": 15, \"nodes\": []}]},"
                                   "  7, \"not an object\"]}]}]}";

static GArray *
find_in_text(
    const char *tree_text
)
{
    g_autoptr(JsonParser) parser = json_parser_new();

    g_assert_true(json_parser_load_from_data(parser, tree_text, -1, NULL));

    return redundant_layout_wrapper_finder_find(json_parser_get_root(parser));
}

/* Only the inner tab group of workspace 1 is reported; windows and workspaces never are. */
static void
test_redundant_wrapper_is_found(void)
{
    GArray *found_ids = find_in_text(example_tree);

    g_assert_cmpuint(found_ids->len, ==, 1);
    g_assert_cmpint(g_array_index(found_ids, int64_t, 0), ==, 7);
    g_array_unref(found_ids);
}

static void
test_unusable_trees_give_nothing(void)
{
    GArray *from_null   = redundant_layout_wrapper_finder_find(NULL);
    GArray *from_number = find_in_text("42");

    g_assert_cmpuint(from_null->len, ==, 0);
    g_assert_cmpuint(from_number->len, ==, 0);
    g_array_unref(from_null);
    g_array_unref(from_number);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/redundant-layout-wrapper-finder/found",    test_redundant_wrapper_is_found },
    {"/redundant-layout-wrapper-finder/unusable", test_unusable_trees_give_nothing},
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
