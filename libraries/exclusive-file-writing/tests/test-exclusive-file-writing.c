#include <unistd.h>

#include <glib/gstdio.h>

#include "exclusive-file-writing.h"

/* A fresh root folder with a writable folder opened somewhere inside it. */
struct test_folder {
    char *root;
    char *path;
    int   descriptor;
};

static struct test_folder
open_test_folder(
    const char *relative_path
)
{
    struct test_folder folder = {.root = g_dir_make_tmp("test-exclusive-file-writing-XXXXXX", NULL)};

    folder.path       = g_build_filename(folder.root, relative_path, NULL);
    folder.descriptor = exclusive_file_writing_open_folder(folder.path, folder.root, NULL);

    g_assert_cmpint(folder.descriptor, >=, 0);

    return folder;
}

static void
close_test_folder(
    struct test_folder *folder
)
{
    close(folder->descriptor);
    g_free(folder->path);
    g_free(folder->root);
}

static bool
write_text(
    const struct test_folder *folder,
    const char               *file_name,
    const char               *text,
    GError                  **error
)
{
    g_autoptr(GBytes) contents = g_bytes_new(text, strlen(text));

    return exclusive_file_writing_write(folder->descriptor, file_name, contents, 0600, error);
}

static char *
read_text(
    const struct test_folder *folder,
    const char               *file_name
)
{
    g_autofree char *path = g_build_filename(folder->path, file_name, NULL);

    char *contents = NULL;

    g_assert_true(g_file_get_contents(path, &contents, NULL, NULL));

    return contents;
}

/* Missing folders are created on the way. */
static void
test_new_file_is_written(void)
{
    struct test_folder folder = open_test_folder("Pictures/Screenshots");

    g_assert_true(write_text(&folder, "shot.png", "first", NULL));

    g_autofree char *text = read_text(&folder, "shot.png");

    g_assert_cmpstr(text, ==, "first");
    close_test_folder(&folder);
}

static void
assert_name_taken(
    const struct test_folder *folder,
    const char               *file_name
)
{
    g_autoptr(GError) error = NULL;

    g_assert_false(write_text(folder, file_name, "second", &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_EXISTS);
}

/* A taken name is reported, so the caller can try the next one; the old file stays. */
static void
test_taken_name_is_never_replaced(void)
{
    struct test_folder folder = open_test_folder("Screenshots");

    g_assert_true(write_text(&folder, "shot.png", "first", NULL));
    assert_name_taken(&folder, "shot.png");

    g_autofree char *text = read_text(&folder, "shot.png");

    g_assert_cmpstr(text, ==, "first");
    close_test_folder(&folder);
}

static void
test_names_must_be_plain(void)
{
    struct test_folder folder = open_test_folder("Screenshots");

    g_assert_false(write_text(&folder, "../escape.png", "x", NULL));
    g_assert_false(write_text(&folder, "..", "x", NULL));
    g_assert_false(write_text(&folder, "", "x", NULL));
    close_test_folder(&folder);
}

/* A symbolic link inside the root that points outside it is refused, before anything is created. */
static void
test_links_leaving_the_root_are_refused(void)
{
    g_autofree char *root    = g_dir_make_tmp("test-exclusive-file-writing-XXXXXX", NULL);
    g_autofree char *outside = g_dir_make_tmp("test-exclusive-file-writing-outside-XXXXXX", NULL);
    g_autofree char *link    = g_build_filename(root, "Pictures", NULL);
    g_autofree char *folder  = g_build_filename(link, "Screenshots", NULL);

    g_assert_cmpint(symlink(outside, link), ==, 0);
    g_assert_cmpint(exclusive_file_writing_open_folder(folder, root, NULL), ==, -1);
    g_assert_true(g_rmdir(outside) == 0); // still empty: nothing was created there
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/exclusive-file-writing/new-file",    test_new_file_is_written               },
    {"/exclusive-file-writing/taken-name",  test_taken_name_is_never_replaced      },
    {"/exclusive-file-writing/plain-names", test_names_must_be_plain               },
    {"/exclusive-file-writing/links",       test_links_leaving_the_root_are_refused},
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
