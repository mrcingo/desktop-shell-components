#include <glib.h>
#include <glib/gstdio.h>

#include "keyboard-layout-short-names.h"

static const char
    example_rules[] = "<?xml version=\"1.0\"?>\n"
                      "<xkbConfigRegistry><layoutList>\n"
                      "<layout><configItem><name>us</name><description>English (US)</description></configItem></layout>\n"
                      "<layout><configItem><name>es</name><description>Spanish</description></configItem>\n"
                      "<variantList><variant><configItem><name>dvorak</name>"
                      "<description>Spanish (Dvorak)</description></configItem></variant></variantList></layout>\n"
                      "</layoutList></xkbConfigRegistry>\n";

static char *
write_example_rules(void)
{
    char *path = NULL;
    int   file = g_file_open_tmp("rules-XXXXXX.xml", &path, NULL);

    g_assert_cmpint(file, >=, 0);
    g_close(file, NULL);
    g_assert_true(g_file_set_contents(path, example_rules, -1, NULL));

    return path;
}

static void
assert_short_name(
    const char *rules_path,
    const char *layout_description,
    const char *expected_short_name
)
{
    g_autofree char *short_name = keyboard_layout_short_name_find(rules_path, layout_description);

    g_assert_cmpstr(short_name, ==, expected_short_name);
}

static void
test_layouts_and_variants_are_found(void)
{
    g_autofree char *rules_path = write_example_rules();

    assert_short_name(rules_path, "Spanish", "es");
    assert_short_name(rules_path, "Spanish (Dvorak)", "es");
    assert_short_name(rules_path, "English (US)", "us");
    assert_short_name(rules_path, "Klingon", NULL);
    g_remove(rules_path);
}

static void
test_missing_rules_find_nothing(void)
{
    assert_short_name("/nonexistent-rules.xml", "Spanish", NULL);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/keyboard-layout-short-names/found",   test_layouts_and_variants_are_found},
    {"/keyboard-layout-short-names/missing", test_missing_rules_find_nothing    },
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
