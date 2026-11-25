#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "managed-file-writing.h"

static const char marker[] = "/* Written by a test; changes here are replaced. */";

static char *
read_file(
    const char *directory,
    const char *relative_path
)
{
    g_autofree char *path = g_build_filename(directory, relative_path, NULL);

    char *contents = NULL;

    g_file_get_contents(path, &contents, NULL, NULL);

    return contents;
}

static void
write_file(
    const char *directory,
    const char *relative_path,
    const char *contents
)
{
    g_autofree char *path = g_build_filename(directory, relative_path, NULL);

    g_file_set_contents(path, contents, -1, NULL);
}

/* A new file is written, an equal one is left as it is, a changed one replaced. */
static void
test_managed_file_is_kept_current(void)
{
    g_autofree char *directory = g_dir_make_tmp("managed-file-XXXXXX", NULL);

    g_assert_cmpint(managed_file_write(directory, "gtk-3.0/gtk.css", marker, "a {}\n", NULL), ==, MANAGED_FILE_WRITTEN);
    g_assert_cmpint(managed_file_write(directory, "gtk-3.0/gtk.css", marker, "a {}\n", NULL), ==, MANAGED_FILE_UNCHANGED);
    g_assert_cmpint(managed_file_write(directory, "gtk-3.0/gtk.css", marker, "b {}\n", NULL), ==, MANAGED_FILE_WRITTEN);

    g_autofree char *contents = read_file(directory, "gtk-3.0/gtk.css");

    g_assert_cmpstr(contents, ==, "/* Written by a test; changes here are replaced. */\nb {}\n");
}

static void
test_user_file_is_left_alone(void)
{
    g_autofree char *directory = g_dir_make_tmp("managed-file-XXXXXX", NULL);

    write_file(directory, "mine.css", "/* my own */\n");
    g_assert_cmpint(managed_file_write(directory, "mine.css", marker, "a {}\n", NULL), ==, MANAGED_FILE_LEFT_TO_USER);

    g_autofree char *contents = read_file(directory, "mine.css");

    g_assert_cmpstr(contents, ==, "/* my own */\n");
}

static void
make_link(
    const char *target,
    const char *directory,
    const char *link_name
)
{
    g_autofree char *link_path = g_build_filename(directory, link_name, NULL);

    g_assert_cmpint(symlink(target, link_path), ==, 0);
}

/* A link is never written through: not a linked file, not a folder linked to outside. */
static void
test_links_are_refused(void)
{
    g_autofree char *directory = g_dir_make_tmp("managed-file-XXXXXX", NULL);
    g_autofree char *outside   = g_dir_make_tmp("managed-file-outside-XXXXXX", NULL);

    make_link("/etc/hostname", directory, "linked.css");
    make_link(outside, directory, "linked-folder");
    g_assert_cmpint(managed_file_write(directory, "linked.css", marker, "a {}\n", NULL), ==, MANAGED_FILE_LEFT_TO_USER);
    g_assert_cmpint(managed_file_write(directory, "linked-folder/gtk.css", marker, "a {}\n", NULL), ==, MANAGED_FILE_FAILED);
}

static void
test_paths_must_stay_inside(void)
{
    g_autofree char *directory = g_dir_make_tmp("managed-file-XXXXXX", NULL);

    g_assert_cmpint(managed_file_write(directory, "../escape.css", marker, "", NULL), ==, MANAGED_FILE_FAILED);
    g_assert_cmpint(managed_file_write(directory, "/tmp/escape.css", marker, "", NULL), ==, MANAGED_FILE_FAILED);
    g_assert_cmpint(managed_file_write(directory, "a/../../escape.css", marker, "", NULL), ==, MANAGED_FILE_FAILED);
}

static enum managed_file_outcome
set_font_name(
    const char *directory
)
{
    const char *const keys[]   = {"gtk-font-name", NULL};
    const char *const values[] = {"Monaspace Neon 10", NULL};

    return managed_file_set_key_file_values(directory, "settings.ini", "Settings", keys, values, NULL);
}

/* Only the named keys change; the user's other settings and comments stay. */
static void
test_key_file_values_are_merged(void)
{
    g_autofree char *directory = g_dir_make_tmp("managed-file-XXXXXX", NULL);

    write_file(directory, "settings.ini", "# mine\n[Settings]\ngtk-icon-theme-name=Papirus\n");
    g_assert_cmpint(set_font_name(directory), ==, MANAGED_FILE_WRITTEN);

    g_autofree char *contents = read_file(directory, "settings.ini");

    g_assert_nonnull(strstr(contents, "# mine"));
    g_assert_nonnull(strstr(contents, "gtk-icon-theme-name=Papirus"));
    g_assert_nonnull(strstr(contents, "gtk-font-name=Monaspace Neon 10"));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/managed-file-writing/kept-current", test_managed_file_is_kept_current},
    {"/managed-file-writing/user-file",    test_user_file_is_left_alone     },
    {"/managed-file-writing/links",        test_links_are_refused           },
    {"/managed-file-writing/paths",        test_paths_must_stay_inside      },
    {"/managed-file-writing/key-file",     test_key_file_values_are_merged  },
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
