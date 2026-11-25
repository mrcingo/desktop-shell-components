#include <string.h>

#include <glib.h>

#include "toolkit-style-sheet-writer.h"

static void
assert_fully_expanded(
    const char *style_sheet
)
{
    g_assert_null(strchr(style_sheet, '$'));
    g_assert_nonnull(strstr(style_sheet, "#005577"));
    g_assert_nonnull(strstr(style_sheet, "#222222"));
}

static void
test_both_versions_use_the_palette(void)
{
    struct theme_palette *palette = theme_palette_create_default();

    g_autofree char *version_3 = toolkit_style_sheet_write_version_3(palette);
    g_autofree char *version_4 = toolkit_style_sheet_write_version_4(palette);

    assert_fully_expanded(version_3);
    assert_fully_expanded(version_4);
    g_assert_nonnull(strstr(version_3, "font-size: 10pt;"));
    theme_palette_destroy(palette);
}

/* A user style sheet outranks every program's own, so the GTK 4 sheet must not style widgets. */
static void
test_version_4_only_defines_colors(void)
{
    struct theme_palette *palette = theme_palette_create_default();

    g_autofree char *version_4 = toolkit_style_sheet_write_version_4(palette);

    g_assert_null(strstr(version_4, "button"));
    g_assert_null(strstr(version_4, "window {"));
    g_assert_nonnull(strstr(version_4, "--window-bg-color: #222222;"));
    theme_palette_destroy(palette);
}

static void
test_font_family_cannot_break_out(void)
{
    g_autofree char *safe_family = toolkit_style_sheet_safe_font_family("Evil\"; } * { color: red; /*");

    g_assert_cmpstr(safe_family, ==, "Evil    color red ");
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/toolkit-style-sheet-writer/palette",     test_both_versions_use_the_palette},
    {"/toolkit-style-sheet-writer/colors-only", test_version_4_only_defines_colors},
    {"/toolkit-style-sheet-writer/font-family", test_font_family_cannot_break_out },
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
