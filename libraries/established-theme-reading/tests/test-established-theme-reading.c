#include <glib.h>

#include "established-theme-reading.h"

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

/* The bar's colors and font apply; the ones it leaves unset keep their defaults. */
static void
test_bar_appearance_is_applied(void)
{
    struct theme_palette            *palette    = theme_palette_create_default();
    struct compositor_bar_appearance appearance = {
        .font             = "pango:Monaspace Neon 10",
        .background_color = "#101010ff",
        .accent_color     = "#aa0000"
    };

    established_theme_apply_bar_appearance(palette, &appearance);
    assert_role_color(palette, THEME_PALETTE_BACKGROUND, "#101010");
    assert_role_color(palette, THEME_PALETTE_ACCENT, "#aa0000");
    g_assert_cmpstr(palette->font_family, ==, "Monaspace Neon");
    theme_palette_destroy(palette);
}

/* Without a compositor the user's own shared-theme.css still applies over the defaults. */
static void
test_user_style_sheet_wins_without_bar(void)
{
    g_autofree char *directory = g_dir_make_tmp("established-theme-XXXXXX", NULL);
    g_autofree char *path      = g_build_filename(directory, "shared-theme.css", NULL);

    g_file_set_contents(path, ":root { --theme-accent-color: #123456; }", -1, NULL);

    struct theme_palette *palette = established_theme_read("/nonexistent/socket", path, 200);

    assert_role_color(palette, THEME_PALETTE_ACCENT, "#123456");
    assert_role_color(palette, THEME_PALETTE_BACKGROUND, "#222222");
    theme_palette_destroy(palette);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/established-theme-reading/bar",        test_bar_appearance_is_applied        },
    {"/established-theme-reading/user-sheet", test_user_style_sheet_wins_without_bar},
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
