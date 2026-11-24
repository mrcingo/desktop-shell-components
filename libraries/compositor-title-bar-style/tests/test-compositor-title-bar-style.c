#include <glib.h>

#include "compositor-title-bar-style.h"

static void
test_configured_style_is_read(void)
{
    struct compositor_title_bar_style *style = compositor_title_bar_style_parse(
        "font pango:Monaspace Neon 10\ntitlebar_padding 7 2\ntitlebar_border_thickness 2\n"
    );

    g_assert_cmpstr(style->font_description, ==, "Monaspace Neon 10");
    g_assert_cmpint(style->horizontal_padding, ==, 7);
    g_assert_cmpint(style->border_thickness, ==, 2);
    g_assert_cmpint(compositor_title_bar_style_text_space(style, 100), ==, 82);
    compositor_title_bar_style_destroy(style);
}

/* Unset or invalid values keep sway's defaults, and quotes around the family go. */
static void
test_defaults_fill_gaps(void)
{
    struct compositor_title_bar_style *style = compositor_title_bar_style_parse(
        "font \"Fira Sans\" 11\ntitlebar_padding wide\n"
    );

    g_assert_cmpstr(style->font_description, ==, "Fira Sans 11");
    g_assert_cmpint(style->horizontal_padding, ==, 5);
    g_assert_cmpint(style->border_thickness, ==, 1);
    g_assert_cmpint(compositor_title_bar_style_text_space(style, 4), ==, 0);
    compositor_title_bar_style_destroy(style);
}

static void
test_missing_font_uses_default(void)
{
    struct compositor_title_bar_style *style = compositor_title_bar_style_parse("");

    g_assert_cmpstr(style->font_description, ==, "monospace 10");
    compositor_title_bar_style_destroy(style);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/compositor-title-bar-style/configured",   test_configured_style_is_read },
    {"/compositor-title-bar-style/defaults",     test_defaults_fill_gaps       },
    {"/compositor-title-bar-style/missing-font", test_missing_font_uses_default},
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
