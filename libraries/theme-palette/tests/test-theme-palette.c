#include <glib.h>

#include "theme-palette.h"

static void
assert_formats_as(
    const char *text,
    const char *expected
)
{
    struct palette_color color;

    g_assert_true(palette_color_parse(text, &color));

    g_autofree char *formatted = palette_color_format(color);

    g_assert_cmpstr(formatted, ==, expected);
}

static void
test_color_forms_are_read(void)
{
    assert_formats_as("#222222", "#222222");
    assert_formats_as("#f80", "#ff8800");
    assert_formats_as("#005577ff", "#005577");
    assert_formats_as("#00557780", "rgba(0, 85, 119, 0.502)");
}

static void
test_bad_colors_are_refused(void)
{
    const char *const    bad_texts[] = {"", "#", "222222", "#12345", "#gggggg", "@theme_bg_color", "#1234567890", "red"};
    struct palette_color color;

    for (size_t i = 0; i < G_N_ELEMENTS(bad_texts); i++) {
        g_assert_false(palette_color_parse(bad_texts[i], &color));
    }

    g_assert_false(palette_color_parse(NULL, &color));
}

static void
test_mixing_and_luminance(void)
{
    struct palette_color black = {0, 0, 0, 1};
    struct palette_color white = {1, 1, 1, 1};

    g_autofree char *grey = palette_color_format(palette_color_mix(black, white, 0.5));

    g_assert_cmpstr(grey, ==, "#808080");
    g_assert_cmpfloat_with_epsilon(palette_color_luminance(white), 1.0, 1e-9);
    g_assert_cmpfloat_with_epsilon(palette_color_luminance(black), 0.0, 1e-9);
}

static void
test_fonts_are_read(void)
{
    struct theme_palette *palette = theme_palette_create_default();

    g_assert_true(theme_palette_set_font(palette, "pango:Monaspace Neon 11"));
    g_assert_cmpstr(palette->font_family, ==, "Monaspace Neon");
    g_assert_true(theme_palette_set_font(palette, "\"Fira Sans\""));
    g_assert_cmpstr(palette->font_family, ==, "Fira Sans");
    g_assert_cmpint(palette->font_size, ==, 11); // kept from before
    theme_palette_destroy(palette);
}

/* Control characters and quotes could break other programs' files, so they go. */
static void
test_odd_fonts_are_cleaned(void)
{
    struct theme_palette *palette = theme_palette_create_default();

    g_assert_false(theme_palette_set_font(palette, "  "));
    g_assert_true(theme_palette_set_font(palette, "Evil\nkey=value 9"));
    g_assert_cmpstr(palette->font_family, ==, "Evilkeyvalue");
    theme_palette_destroy(palette);
}

static const char user_style_sheet[] = ":root {\n"
                                       "    --theme-background-color: #101010;\n"
                                       "    /* --theme-accent-color: #ff0000; */\n"
                                       "    --theme-accent-color: @theme_selected_bg_color;\n"
                                       "    --theme-text-color:#fafafa}\n"
                                       ".x { color: var(--theme-text-color); }\n";

static void
assert_role_color(
    const struct theme_palette *palette,
    enum theme_palette_role     role,
    const char                 *expected
)
{
    g_autofree char *formatted = palette_color_format(palette->colors[role]);

    g_assert_cmpstr(formatted, ==, expected);
}

/* Plain colors apply; comments, references and uses of the variable do not. */
static void
test_variables_are_applied(void)
{
    struct theme_palette *palette = theme_palette_create_default();

    theme_palette_apply_variables(palette, user_style_sheet);

    assert_role_color(palette, THEME_PALETTE_BACKGROUND, "#101010");
    assert_role_color(palette, THEME_PALETTE_ACCENT, "#005577");
    assert_role_color(palette, THEME_PALETTE_TEXT, "#fafafa");
    theme_palette_destroy(palette);
}

static void
test_darkness_follows_background(void)
{
    struct theme_palette *palette = theme_palette_create_default();

    g_assert_true(theme_palette_is_dark(palette));
    theme_palette_set_color(palette, THEME_PALETTE_BACKGROUND, "#f0f0f0");
    g_assert_false(theme_palette_is_dark(palette));
    theme_palette_destroy(palette);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/theme-palette/color-forms", test_color_forms_are_read       },
    {"/theme-palette/bad-colors",  test_bad_colors_are_refused     },
    {"/theme-palette/mixing",      test_mixing_and_luminance       },
    {"/theme-palette/fonts",       test_fonts_are_read             },
    {"/theme-palette/odd-fonts",   test_odd_fonts_are_cleaned      },
    {"/theme-palette/variables",   test_variables_are_applied      },
    {"/theme-palette/darkness",    test_darkness_follows_background},
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
