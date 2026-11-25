#include <unistd.h>

#include <glib/gstdio.h>

#include "program-configuration-file.h"

static const char *const position_choices[] = {"bottom", "top", NULL};

static const struct program_configuration_key known_keys[] = {
    {"bar", "position"},
    {"bar", "height"  },
    {"bar", "label"   },
    {"bar", "previews"},
    {NULL,  NULL      },
};

struct loaded_configuration {
    char                              *path;
    struct program_configuration_file *file;
};

static struct loaded_configuration
load_contents(
    const char *contents
)
{
    struct loaded_configuration loaded = {NULL, NULL};
    int                         file   = g_file_open_tmp("program-configuration-file-test-XXXXXX.ini", &loaded.path, NULL);

    g_assert_cmpint(file, >=, 0);
    g_assert_true(g_file_set_contents(loaded.path, contents, -1, NULL));
    close(file);

    loaded.file = program_configuration_file_load("example-program", loaded.path);

    return loaded;
}

static void
unload(
    struct loaded_configuration *loaded
)
{
    program_configuration_file_destroy(loaded->file);
    g_unlink(loaded->path);
    g_free(loaded->path);
}

static void
test_missing_file_keeps_every_default(void)
{
    struct program_configuration_file *configuration = program_configuration_file_load(
        "program-without-any-configuration",
        NULL
    );

    g_autofree char *label = program_configuration_file_read_text(configuration, "bar", "label", "{name}");

    g_assert_null(program_configuration_file_get_path(configuration));
    g_assert_cmpstr(label, ==, "{name}");
    g_assert_cmpint(program_configuration_file_read_integer(configuration, "bar", "height", 22, 10, 200), ==, 22);
    g_assert_cmpint(program_configuration_file_read_choice(configuration, "bar", "position", position_choices, 0), ==, 0);
    program_configuration_file_destroy(configuration);
}

static void
test_valid_values_are_read(void)
{
    struct loaded_configuration loaded = load_contents("[bar]\nposition = top\nheight = 30\nlabel = {pid}\n");

    g_autofree char *label = program_configuration_file_read_text(loaded.file, "bar", "label", "{name}");

    g_assert_cmpstr(label, ==, "{pid}");
    g_assert_cmpint(program_configuration_file_read_integer(loaded.file, "bar", "height", 22, 10, 200), ==, 30);
    g_assert_cmpint(program_configuration_file_read_choice(loaded.file, "bar", "position", position_choices, 0), ==, 1);
    unload(&loaded);
}

static void
test_invalid_values_fall_back(void)
{
    struct loaded_configuration loaded = load_contents("[bar]\nposition = left\nheight = 900\n");

    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*[bar] position must be bottom or top*");
    g_assert_cmpint(program_configuration_file_read_choice(loaded.file, "bar", "position", position_choices, 0), ==, 0);
    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*[bar] height must be a whole number from 10 to 200*");
    g_assert_cmpint(program_configuration_file_read_integer(loaded.file, "bar", "height", 22, 10, 200), ==, 22);
    unload(&loaded);
}

static void
test_unknown_keys_are_reported(void)
{
    struct loaded_configuration loaded = load_contents("[bar]\nprevious = x\n");

    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*[bar] has no setting \"previous\"*");
    program_configuration_file_report_unknown_keys(loaded.file, known_keys);
    g_test_assert_expected_messages();
    unload(&loaded);
}

static const struct program_configuration_key rule_keys[] = {
    {"app:*", "timeout"},
    {NULL,    NULL     },
};

static void
assert_second_group(
    struct program_configuration_file *file,
    const char                        *prefix,
    const char                        *expected_name
)
{
    g_auto(GStrv) groups = program_configuration_file_list_groups_with_prefix(file, prefix);

    g_assert_cmpstr(groups[1], ==, expected_name);
}

/* [app:*] stands for every application group; other groups keep exact names. */
static void
test_prefix_groups_are_listed_and_checked(void)
{
    struct loaded_configuration loaded = load_contents("[app:Slack]\ntimeout = 3\n[app:foot]\ntimeot = 1\n[apps]\nx = 1\n");

    assert_second_group(loaded.file, "app:", "app:foot");
    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*[app:foot] has no setting \"timeot\"*");
    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*[apps] has no setting \"x\"*");
    program_configuration_file_report_unknown_keys(loaded.file, rule_keys);
    g_test_assert_expected_messages();
    unload(&loaded);
}

static void
test_booleans_accept_true_and_false_only(void)
{
    struct loaded_configuration loaded = load_contents("[switcher]\npreviews = false\nclose = maybe\n");

    g_assert_false(program_configuration_file_read_boolean(loaded.file, "switcher", "previews", true));
    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*[switcher] close must be true or false*");
    g_assert_true(program_configuration_file_read_boolean(loaded.file, "switcher", "close", true));
    unload(&loaded);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/program-configuration-file/missing-file",   test_missing_file_keeps_every_default    },
    {"/program-configuration-file/valid-values",   test_valid_values_are_read               },
    {"/program-configuration-file/invalid-values", test_invalid_values_fall_back            },
    {"/program-configuration-file/unknown-keys",   test_unknown_keys_are_reported           },
    {"/program-configuration-file/booleans",       test_booleans_accept_true_and_false_only },
    {"/program-configuration-file/prefix-groups",  test_prefix_groups_are_listed_and_checked},
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
