#include <string.h>

#include "compositor-bar-appearance.h"

static const char bar_configuration[] = "{\"id\": \"bar-0\", \"font\": \"pango:Monaspace Neon 10\", \"colors\": {"
                                        "\"background\": \"#222222ff\", \"statusline\": \"#bbbbbbff\","
                                        "\"inactive_workspace_text\": \"#bbbbbbff\", \"focused_workspace_bg\": \"#005577ff\","
                                        "\"focused_workspace_text\": \"#eeeeeeff\", \"active_workspace_border\": \"#444444ff\","
                                        "\"urgent_workspace_bg\": \"#900000ff\"}}";

static struct compositor_bar_appearance *
parse_text(
    JsonParser *parser,
    const char *text
)
{
    g_assert_true(json_parser_load_from_data(parser, text, -1, NULL));

    return compositor_bar_appearance_parse(json_parser_get_root(parser));
}

static void
assert_colors_and_font(
    const struct compositor_bar_appearance *appearance
)
{
    g_assert_cmpstr(appearance->font, ==, "Monaspace Neon 10");
    g_assert_cmpstr(appearance->background_color, ==, "#222222ff");
    g_assert_cmpstr(appearance->accent_color, ==, "#005577ff");
    g_assert_cmpstr(appearance->accent_text_color, ==, "#eeeeeeff");
}

static void
assert_style_sheet(
    const char *style_sheet
)
{
    g_assert_nonnull(strstr(style_sheet, "--theme-accent-color: #005577ff;"));
    g_assert_nonnull(strstr(style_sheet, "font-family: \"Monaspace Neon\";"));
    g_assert_nonnull(strstr(style_sheet, "font-size: 10pt;"));
}

static void
test_colors_and_font_are_read(void)
{
    JsonParser                       *parser     = json_parser_new();
    struct compositor_bar_appearance *appearance = parse_text(parser, bar_configuration);

    assert_colors_and_font(appearance);
    compositor_bar_appearance_destroy(appearance);
    g_object_unref(parser);
}

static void
test_style_sheet_sets_variables_and_font(void)
{
    JsonParser                       *parser     = json_parser_new();
    struct compositor_bar_appearance *appearance = parse_text(parser, bar_configuration);

    g_autofree char *style_sheet = compositor_bar_appearance_to_style_sheet(appearance);

    assert_style_sheet(style_sheet);
    compositor_bar_appearance_destroy(appearance);
    g_object_unref(parser);
}

/* A bar that sets nothing adds nothing, so the GTK theme stays in charge. */
static void
test_unset_values_add_nothing(void)
{
    JsonParser                       *parser     = json_parser_new();
    struct compositor_bar_appearance *appearance = parse_text(parser, "{\"id\": \"bar-0\"}");

    g_autofree char *style_sheet = compositor_bar_appearance_to_style_sheet(appearance);

    g_assert_null(appearance->font);
    g_assert_cmpstr(style_sheet, ==, ":root {\n}\n");
    compositor_bar_appearance_destroy(appearance);
    g_object_unref(parser);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/compositor-bar-appearance/colors-and-font", test_colors_and_font_are_read           },
    {"/compositor-bar-appearance/style-sheet",     test_style_sheet_sets_variables_and_font},
    {"/compositor-bar-appearance/unset-values",    test_unset_values_add_nothing           },
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
