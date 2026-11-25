#include <glib.h>
#include <glib/gstdio.h>

#include "display-backlight-level.h"

static void
write_file(
    const char *directory,
    const char *file_name,
    const char *contents
)
{
    g_autofree char *path = g_build_filename(directory, file_name, NULL);

    g_assert_true(g_file_set_contents(path, contents, -1, NULL));
}

static char *
create_fake_backlight(void)
{
    char *root = g_dir_make_tmp("backlight-XXXXXX", NULL);

    g_autofree char *device = g_build_filename(root, "panel", NULL);

    g_mkdir(device, 0700);
    write_file(device, "max_brightness", "255\n");
    write_file(device, "actual_brightness", "128\n");
    write_file(device, "brightness", "128\n");

    return root;
}

static const char *const fake_files[] = {"panel/max_brightness", "panel/actual_brightness", "panel/brightness", "panel", ""};

static void
remove_fake_backlight(
    const char *root
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(fake_files); i++) {
        g_autofree char *path = g_build_filename(root, fake_files[i], NULL);

        g_assert_cmpint(g_remove(path), ==, 0);
    }
}

static void
assert_file_holds(
    const char *root,
    const char *expected_contents
)
{
    g_autofree char *path     = g_build_filename(root, "panel", "brightness", NULL);
    g_autofree char *contents = NULL;

    g_assert_true(g_file_get_contents(path, &contents, NULL, NULL));
    g_assert_cmpstr(contents, ==, expected_contents);
}

/* Asking for 0% sets the lowest visible level, 1. */
static void
test_level_is_read_and_written(void)
{
    g_autofree char *root = create_fake_backlight();

    struct display_backlight *backlight = display_backlight_find(root);

    g_assert_cmpint(display_backlight_read_percent(backlight), ==, 50);
    g_assert_true(display_backlight_set_percent(backlight, 0));
    assert_file_holds(root, "1\n");
    display_backlight_destroy(backlight);
    remove_fake_backlight(root);
}

static void
test_missing_backlight_is_reported(void)
{
    g_assert_null(display_backlight_find("/nonexistent-backlight-class"));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/display-backlight-level/read-write", test_level_is_read_and_written    },
    {"/display-backlight-level/missing",    test_missing_backlight_is_reported},
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
