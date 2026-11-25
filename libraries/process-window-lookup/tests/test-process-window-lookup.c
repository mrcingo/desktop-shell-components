#include <glib/gstdio.h>

#include "process-window-lookup.h"

static char *no_marks[] = {NULL};

static struct compositor_window *
create_window(
    int64_t     container_id,
    const char *application_name,
    int         process_id
)
{
    struct compositor_window *window = g_new0(struct compositor_window, 1);

    *window = (struct compositor_window){
        .container_id     = container_id,
        .application_name = g_strdup(application_name),
        .title            = g_strdup(""),
        .workspace_name   = g_strdup("1"),
        .process_id       = process_id,
        .marks            = g_strdupv(no_marks),
    };

    return window;
}

static void
destroy_window(
    gpointer data
)
{
    struct compositor_window *window = data;

    g_free(window->application_name);
    g_free(window->title);
    g_free(window->workspace_name);
    g_strfreev(window->marks);
    g_free(window);
}

/* Windows of a terminal (100), a browser (300, whose page renderer 301 sent nothing) and Slack (400). */
static struct compositor_window_tree
create_tree(void)
{
    GPtrArray *windows = g_ptr_array_new_with_free_func(destroy_window);

    g_ptr_array_add(windows, create_window(1, "foot", 100));
    g_ptr_array_add(windows, create_window(2, "firefox", 300));
    g_ptr_array_add(windows, create_window(3, "Slack", 400));

    return (struct compositor_window_tree){.all_windows = windows};
}

static void
write_process(
    const char *proc_directory,
    int         process_id,
    int         parent_id
)
{
    g_autofree char *process_directory = g_strdup_printf("%s/%d", proc_directory, process_id);
    g_autofree char *stat_path         = g_build_filename(process_directory, "stat", NULL);
    g_autofree char *stat_text         = g_strdup_printf("%d (odd) name) S %d 1 1\n", process_id, parent_id);

    g_mkdir(process_directory, 0700);
    g_file_set_contents(stat_path, stat_text, -1, NULL);
}

/* 50 started 100 (foot), which runs a shell 150, which runs notify-send 151; 300 started 301. */
static char *
create_fake_proc_directory(void)
{
    static const int processes[][2] = {
        {50,  1  },
        {100, 50 },
        {150, 100},
        {151, 150},
        {300, 1  },
        {301, 300}
    };
    char *proc_directory = g_dir_make_tmp("process-window-lookup-XXXXXX", NULL);

    for (size_t i = 0; i < G_N_ELEMENTS(processes); i++) {
        write_process(proc_directory, processes[i][0], processes[i][1]);
    }

    return proc_directory;
}

static void
test_windows_are_found_by_process(void)
{
    g_autofree char *proc_directory = create_fake_proc_directory();

    struct compositor_window_tree tree = create_tree();

    g_assert_cmpint(process_window_lookup_find(&tree, 151, "notify-send", proc_directory)->container_id, ==, 1);
    g_assert_cmpint(process_window_lookup_find(&tree, 301, NULL, proc_directory)->container_id, ==, 2);
    g_assert_cmpint(process_window_lookup_find(&tree, 400, NULL, proc_directory)->container_id, ==, 3);
    g_ptr_array_unref(tree.all_windows);
}

/* A child that sends for its parent, such as a helper process, finds the parent's window. */
static void
test_windows_are_found_by_descendant_and_name(void)
{
    g_autofree char *proc_directory = create_fake_proc_directory();

    struct compositor_window_tree tree = create_tree();

    g_assert_cmpint(process_window_lookup_find(&tree, 50, NULL, proc_directory)->container_id, ==, 1);
    g_assert_cmpint(process_window_lookup_find(&tree, 0, "slack", proc_directory)->container_id, ==, 3);
    g_assert_null(process_window_lookup_find(&tree, 999, "unknown", proc_directory));
    g_assert_null(process_window_lookup_find(&tree, 1, NULL, proc_directory));
    g_ptr_array_unref(tree.all_windows);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/process-window-lookup/process",         test_windows_are_found_by_process            },
    {"/process-window-lookup/descendant-name", test_windows_are_found_by_descendant_and_name},
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
