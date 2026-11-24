#include <glib.h>

#include "replaced-executable-restart.h"

static const struct {
    const char *link_target;
    const char *expected_path;
} path_cases[] = {
    {"/usr/bin/desktop-shell-window-switcher (deleted)", "/usr/bin/desktop-shell-window-switcher"},
    {"/usr/bin/desktop-shell-window-switcher",           NULL                                    },
    {"/tmp/odd (deleted) name",                          NULL                                    },
};

static void
assert_new_path(
    size_t case_index
)
{
    g_autofree char *new_path = replaced_executable_find_new_path(path_cases[case_index].link_target);

    g_assert_cmpstr(new_path, ==, path_cases[case_index].expected_path);
}

static void
test_deleted_files_are_recognised(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(path_cases); i++) {
        assert_new_path(i);
    }
}

/* The test program itself was not replaced, so this must return. */
static void
test_unreplaced_program_keeps_running(void)
{
    char                               *arguments[] = {"test", NULL};
    struct replaced_executable_restart *restart     = replaced_executable_restart_create(1, arguments);

    replaced_executable_restart_if_replaced(restart);
    replaced_executable_restart_destroy(restart);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/replaced-executable-restart/deleted-paths", test_deleted_files_are_recognised    },
    {"/replaced-executable-restart/unreplaced",    test_unreplaced_program_keeps_running},
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
