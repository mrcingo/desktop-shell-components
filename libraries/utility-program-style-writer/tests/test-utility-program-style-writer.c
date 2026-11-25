#include <string.h>

#include <glib.h>

#include "utility-program-style-writer.h"

static void
test_lock_screen_uses_the_palette(void)
{
    struct theme_palette *palette = theme_palette_create_default();

    g_autofree char *text = utility_program_style_write_lock_screen(palette);

    g_assert_nonnull(strstr(text, "color=222222\n"));
    g_assert_nonnull(strstr(text, "ring-ver-color=005577\n"));
    g_assert_nonnull(strstr(text, "line-color=00000000\n"));
    g_assert_null(strchr(text, '#'));
    theme_palette_destroy(palette);
}

static void
test_prompt_bar_has_both_kinds(void)
{
    struct theme_palette *palette = theme_palette_create_default();

    g_autofree char *text = utility_program_style_write_prompt_bar(palette);

    g_assert_nonnull(strstr(text, "[warning]\nbackground=222222\n"));
    g_assert_nonnull(strstr(text, "[error]\n"));
    g_assert_nonnull(strstr(text, "border-bottom=900000\n"));
    theme_palette_destroy(palette);
}

static void
test_launcher_arguments(void)
{
    struct theme_palette *palette = theme_palette_create_default();

    g_auto(GStrv) arguments = utility_program_style_launcher_arguments(palette);

    g_assert_cmpuint(g_strv_length(arguments), ==, 14);
    g_assert_cmpstr(arguments[1], ==, "monospace 10");
    g_assert_cmpstr(arguments[3], ==, "222222");
    theme_palette_destroy(palette);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/utility-program-style-writer/lock-screen", test_lock_screen_uses_the_palette},
    {"/utility-program-style-writer/prompt-bar",  test_prompt_bar_has_both_kinds   },
    {"/utility-program-style-writer/launcher",    test_launcher_arguments          },
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
