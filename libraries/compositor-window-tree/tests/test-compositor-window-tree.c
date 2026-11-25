#include "compositor-window-tree.h"

struct parsed_example_tree {
    JsonParser                    *parser;
    struct compositor_window_tree *tree;
};

struct expected_window {
    int64_t     container_id;
    const char *application_name;
};

static struct compositor_window_tree *
parse_document(
    JsonParser *parser,
    const char *document_text
)
{
    g_assert_true(json_parser_load_from_data(parser, document_text, -1, NULL));

    return compositor_window_tree_parse(json_parser_get_root(parser));
}

static struct parsed_example_tree
load_example_tree(void)
{
    struct parsed_example_tree example = {.parser = json_parser_new()};
    GError                    *error   = NULL;

    g_assert_true(json_parser_load_from_file(example.parser, EXAMPLE_WINDOW_TREE_PATH, &error));
    g_assert_no_error(error);

    example.tree = compositor_window_tree_parse(json_parser_get_root(example.parser));

    g_assert_nonnull(example.tree);

    return example;
}

static void
unload_example_tree(
    struct parsed_example_tree *example
)
{
    compositor_window_tree_destroy(example->tree);
    g_object_unref(example->parser);
}

static const struct compositor_window *
window_at(
    const struct compositor_output *output,
    guint                           index
)
{
    g_assert_cmpuint(index, <, output->windows_on_visible_workspace->len);

    return g_ptr_array_index(output->windows_on_visible_workspace, index);
}

static const struct compositor_window *
example_window(
    struct parsed_example_tree *example,
    const char                 *output_name,
    guint                       index
)
{
    const struct compositor_output *output = compositor_window_tree_find_output_by_name(example->tree, output_name);

    g_assert_nonnull(output);

    return window_at(output, index);
}

static void
assert_window_label(
    const struct compositor_window *window,
    const char                     *expected_label
)
{
    char *label = compositor_window_format_name_and_process_id(window);

    g_assert_cmpstr(label, ==, expected_label);
    g_free(label);
}

static void
assert_windows_in_order(
    const struct compositor_output *output,
    const struct expected_window   *expected_windows,
    guint                           expected_count
)
{
    g_assert_cmpuint(output->windows_on_visible_workspace->len, ==, expected_count);

    for (guint i = 0; i < expected_count; i++) {
        g_assert_cmpint(window_at(output, i)->container_id, ==, expected_windows[i].container_id);
        g_assert_cmpstr(window_at(output, i)->application_name, ==, expected_windows[i].application_name);
    }
}

static void
assert_window_ids_equal(
    GArray        *window_ids,
    const int64_t *expected_ids,
    guint          expected_count
)
{
    g_assert_cmpuint(window_ids->len, ==, expected_count);

    for (guint i = 0; i < expected_count; i++) {
        g_assert_cmpint(g_array_index(window_ids, int64_t, i), ==, expected_ids[i]);
    }
}

static void
assert_laptop_panel_is_unfocused_below_top_bar(
    struct parsed_example_tree *example
)
{
    const struct compositor_output *laptop_panel = compositor_window_tree_find_output_by_name(example->tree, "eDP-1");

    g_assert_nonnull(laptop_panel);
    g_assert_false(laptop_panel->contains_focused_workspace);
    g_assert_cmpint(laptop_panel->space_reserved_above_workspace, ==, 22);
}

static void
assert_focused_output_name(
    struct parsed_example_tree *example,
    const char                 *expected_name
)
{
    const struct compositor_output *focused = compositor_window_tree_find_focused_output(example->tree);

    g_assert_nonnull(focused);
    g_assert_cmpstr(focused->output_name, ==, expected_name);
}

static void
test_outputs_skip_the_hidden_scratchpad_output(void)
{
    struct parsed_example_tree example = load_example_tree();

    g_assert_cmpuint(example.tree->outputs->len, ==, 2);
    g_assert_null(compositor_window_tree_find_output_by_name(example.tree, "__i3"));
    assert_laptop_panel_is_unfocused_below_top_bar(&example);
    assert_focused_output_name(&example, "HDMI-A-1");
    unload_example_tree(&example);
}

/* Tiled windows in layout order, then floating ones; an empty app_id falls back to the title. */
static void
test_windows_of_visible_workspace_in_layout_order(void)
{
    static const struct expected_window expected_windows[] = {
        {11, "google-chrome"},
        {13, "XTerm"        },
        {14, "foot"         },
        {15, "calc"         },
    };

    struct parsed_example_tree      example      = load_example_tree();
    const struct compositor_output *laptop_panel = compositor_window_tree_find_output_by_name(example.tree, "eDP-1");

    assert_windows_in_order(laptop_panel, expected_windows, G_N_ELEMENTS(expected_windows));
    unload_example_tree(&example);
}

static void
test_window_details_are_parsed(void)
{
    struct parsed_example_tree      example = load_example_tree();
    const struct compositor_window *browser = example_window(&example, "eDP-1", 0);

    g_assert_cmpint(browser->process_id, ==, 2210);
    g_assert_cmpint(browser->title_bar_width, ==, 480);
    g_assert_cmpstr(browser->toplevel_capture_identifier, ==, "aaaa");
    assert_window_label(browser, "google-chrome 2210");
    unload_example_tree(&example);
}

static void
test_missing_identifier_and_keyboard_focus(void)
{
    struct parsed_example_tree example = load_example_tree();

    g_assert_null(example_window(&example, "eDP-1", 2)->toplevel_capture_identifier);
    g_assert_cmpint(example_window(&example, "eDP-1", 2)->title_bar_width, ==, 0);
    g_assert_true(example_window(&example, "HDMI-A-1", 0)->has_keyboard_focus);
    unload_example_tree(&example);
}

static void
test_focus_order_follows_focus_lists(void)
{
    static const int64_t expected_order[] = {31, 14, 13, 15, 11, 21};

    struct parsed_example_tree example = load_example_tree();

    assert_window_ids_equal(example.tree->window_ids_in_focus_order, expected_order, G_N_ELEMENTS(expected_order));
    unload_example_tree(&example);
}

static void
test_documents_that_are_not_trees_are_rejected(void)
{
    static const char *const not_trees[] = {
        "[]",
        "{\"type\": \"workspace\", \"nodes\": []}",
        "{\"type\": \"root\"}",
    };

    JsonParser *parser = json_parser_new();

    g_assert_null(compositor_window_tree_parse(NULL));

    for (size_t i = 0; i < G_N_ELEMENTS(not_trees); i++) {
        g_assert_null(parse_document(parser, not_trees[i]));
    }

    g_object_unref(parser);
}

static const char document_with_unnamed_output
    [] = "{\"type\": \"root\", \"nodes\": [{\"type\": \"output\"}, {\"type\": \"output\", \"name\": \"DP-1\"}]}";

static void
test_outputs_without_names_are_skipped(void)
{
    JsonParser                    *parser = json_parser_new();
    struct compositor_window_tree *tree   = parse_document(parser, document_with_unnamed_output);

    g_assert_nonnull(tree);
    g_assert_cmpuint(tree->outputs->len, ==, 1);
    g_assert_null(compositor_window_tree_find_focused_output(tree));
    compositor_window_tree_destroy(tree);
    g_object_unref(parser);
}

static bool
name_comes_from_title(
    JsonParser *parser,
    const char *window_node_json
)
{
    g_assert_true(json_parser_load_from_data(parser, window_node_json, -1, NULL));

    return compositor_window_name_comes_from_title(json_node_get_object(json_parser_get_root(parser)));
}

static void
test_only_unnamed_windows_take_their_title(void)
{
    JsonParser *parser = json_parser_new();

    g_assert_false(name_comes_from_title(parser, "{\"app_id\": \"foot\", \"name\": \"~\"}"));
    g_assert_false(name_comes_from_title(parser, "{\"app_id\": null, \"window_properties\": {\"class\": \"XTerm\"}}"));
    g_assert_true(name_comes_from_title(parser, "{\"app_id\": \"\", \"name\": \"calc\"}"));
    g_object_unref(parser);
}

static const struct compositor_window *
window_in_list(
    GPtrArray *windows,
    guint      window_index
)
{
    g_assert_cmpuint(window_index, <, windows->len);

    return g_ptr_array_index(windows, window_index);
}

static void
assert_title_and_workspace(
    const struct compositor_window *window,
    const char                     *expected_title,
    const char                     *expected_workspace
)
{
    g_assert_cmpstr(window->title, ==, expected_title);
    g_assert_cmpstr(window->workspace_name, ==, expected_workspace);
}

/* Every workspace of every output, hidden workspaces included, scratchpad excluded. */
static void
test_all_windows_carry_title_and_workspace(void)
{
    struct parsed_example_tree example = load_example_tree();
    GPtrArray                 *windows = example.tree->all_windows;

    g_assert_cmpuint(windows->len, ==, 6);
    assert_title_and_workspace(window_in_list(windows, 0), "Some page", "1");
    assert_title_and_workspace(window_in_list(windows, 4), "hidden", "2");
    unload_example_tree(&example);
}

/* Window states; marks that are not strings are skipped. */
static void
test_window_states_are_parsed(void)
{
    struct parsed_example_tree      example = load_example_tree();
    const struct compositor_window *browser = example_window(&example, "eDP-1", 0);

    g_assert_cmpuint(g_strv_length(browser->marks), ==, 2);
    g_assert_true(browser->is_sticky);
    g_assert_true(browser->is_fullscreen);
    g_assert_false(browser->is_x11_window);
    unload_example_tree(&example);
}

static void
test_marked_windows_are_found(void)
{
    struct parsed_example_tree      example = load_example_tree();
    const struct compositor_window *marked  = compositor_window_tree_find_marked_window(example.tree, "_menu");
    const struct compositor_output *output  = compositor_window_tree_find_output_showing_window(
        example.tree,
        marked->container_id
    );

    g_assert_cmpint(marked->container_id, ==, 11);
    g_assert_true(compositor_window_tree_find_window(example.tree, 11) == marked);
    g_assert_cmpstr(output->output_name, ==, "eDP-1");
    unload_example_tree(&example);
}

/* Sizes and layouts, which components that place windows need. */
static void
test_window_geometry_is_parsed(void)
{
    struct parsed_example_tree      example = load_example_tree();
    const struct compositor_window *browser = compositor_window_tree_find_window(example.tree, 11);

    g_assert_cmpint(browser->content_width * 10000 + browser->content_height, ==, 480 * 10000 + 600);
    g_assert_cmpint(browser->title_bar_height * 100 + browser->border_width, ==, 24 * 100 + 2);
    g_assert_cmpint(browser->parent_layout, ==, COMPOSITOR_CONTAINER_LAYOUT_SIDE_BY_SIDE);
    compositor_window_free(compositor_window_copy(browser));
    unload_example_tree(&example);
}

static void
test_parent_layout_and_visibility(void)
{
    struct parsed_example_tree      example = load_example_tree();
    const struct compositor_window *browser = compositor_window_tree_find_window(example.tree, 11);
    const struct compositor_window *xterm   = compositor_window_tree_find_window(example.tree, 13);

    g_assert_cmpint(xterm->parent_layout, ==, COMPOSITOR_CONTAINER_LAYOUT_TABBED);
    g_assert_true(browser->is_visible);
    g_assert_false(xterm->is_visible);
    unload_example_tree(&example);
}

static void
test_output_areas_are_parsed(void)
{
    struct parsed_example_tree      example = load_example_tree();
    const struct compositor_output *output  = compositor_window_tree_find_output_by_name(example.tree, "eDP-1");
    const struct compositor_window *calc    = compositor_window_tree_find_window(example.tree, 15);

    g_assert_cmpint(output->output_area.width * 10000 + output->output_area.height, ==, 1920 * 10000 + 1200);
    g_assert_cmpint(output->workspace_area.y * 10000 + output->workspace_area.height, ==, 22 * 10000 + 1156);
    g_assert_cmpint(calc->parent_layout, ==, COMPOSITOR_CONTAINER_LAYOUT_UNKNOWN);
    unload_example_tree(&example);
}

static const struct {
    const char *test_path;

    void (*test_function)(void);
} registered_tests[] = {
    {"/compositor-window-tree/outputs",               test_outputs_skip_the_hidden_scratchpad_output   },
    {"/compositor-window-tree/windows",               test_windows_of_visible_workspace_in_layout_order},
    {"/compositor-window-tree/window-details",        test_window_details_are_parsed                   },
    {"/compositor-window-tree/window-states",         test_window_states_are_parsed                    },
    {"/compositor-window-tree/window-geometry",       test_window_geometry_is_parsed                   },
    {"/compositor-window-tree/parent-layout",         test_parent_layout_and_visibility                },
    {"/compositor-window-tree/output-areas",          test_output_areas_are_parsed                     },
    {"/compositor-window-tree/marked-windows",        test_marked_windows_are_found                    },
    {"/compositor-window-tree/identifier-and-focus",  test_missing_identifier_and_keyboard_focus       },
    {"/compositor-window-tree/focus-order",           test_focus_order_follows_focus_lists             },
    {"/compositor-window-tree/not-trees",             test_documents_that_are_not_trees_are_rejected   },
    {"/compositor-window-tree/outputs-without-names", test_outputs_without_names_are_skipped           },
    {"/compositor-window-tree/name-from-title",       test_only_unnamed_windows_take_their_title       },
    {"/compositor-window-tree/all-windows",           test_all_windows_carry_title_and_workspace       },
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
