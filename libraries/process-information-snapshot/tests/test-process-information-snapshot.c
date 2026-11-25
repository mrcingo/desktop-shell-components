#include <glib.h>
#include <glib/gstdio.h>
#include <unistd.h>

#include "process-information-snapshot.h"

static void
write_fake_file(
    const char *process_directory,
    const char *file_name,
    const char *contents,
    gssize      length
)
{
    g_autofree char *path = g_build_filename(process_directory, file_name, NULL);

    g_file_set_contents(path, contents, length, NULL);
}

static void
write_fake_process_files(
    const char *process_directory
)
{
    write_fake_file(process_directory, "cmdline", "foot\0--title\0a b\0", 17);
    write_fake_file(process_directory, "statm", "1000 256 10 1 0 100 0\n", -1);
    write_fake_file(process_directory, "stat", "42 (odd) name) T 1 42 42 0\n", -1);
}

/* A fake /proc with one process, 42, so the test does not depend on the machine. */
static char *
create_fake_proc_directory(void)
{
    char *proc_directory = g_dir_make_tmp("test-process-information-XXXXXX", NULL);

    g_autofree char *process_directory = g_build_filename(proc_directory, "42", NULL);
    g_autofree char *cwd_link          = g_build_filename(process_directory, "cwd", NULL);

    g_mkdir(process_directory, 0700);
    write_fake_process_files(process_directory);
    g_assert_cmpint(symlink("/home", cwd_link), ==, 0);

    return proc_directory;
}

static void
test_snapshot_is_read(void)
{
    g_autofree char *proc_directory = create_fake_proc_directory();

    struct process_information_snapshot *snapshot = process_information_snapshot_read(proc_directory, 42);

    g_assert_cmpstr(snapshot->command_line, ==, "foot --title a b");
    g_assert_cmpstr(snapshot->working_directory, ==, "/home");
    g_assert_cmpuint(snapshot->resident_memory_bytes, ==, 256 * (guint64)sysconf(_SC_PAGESIZE));
    g_assert_true(snapshot->is_stopped);
    process_information_snapshot_destroy(snapshot);
}

static void
test_missing_process_gives_null(void)
{
    g_autofree char *proc_directory = create_fake_proc_directory();

    g_assert_null(process_information_snapshot_read(proc_directory, 43));
    g_assert_null(process_information_snapshot_read(proc_directory, 0));
    g_assert_null(process_information_snapshot_read(proc_directory, -1));
}

static void
test_memory_sizes_are_formatted(void)
{
    g_autofree char *small = process_information_format_memory_size(312 * 1024 * 1024);
    g_autofree char *large = process_information_format_memory_size(1536ull * 1024 * 1024);

    g_assert_cmpstr(small, ==, "312 MiB");
    g_assert_cmpstr(large, ==, "1.5 GiB");
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/process-information-snapshot/read",    test_snapshot_is_read          },
    {"/process-information-snapshot/missing", test_missing_process_gives_null},
    {"/process-information-snapshot/memory",  test_memory_sizes_are_formatted},
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
