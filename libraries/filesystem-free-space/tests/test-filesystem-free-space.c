#include <glib.h>

#include "filesystem-free-space.h"

static void
test_root_filesystem_is_measured(void)
{
    struct filesystem_space space = {0};

    g_assert_true(filesystem_space_read("/", &space));
    g_assert_cmpuint(space.total_bytes, >, 0);
    g_assert_cmpuint(space.available_bytes, <=, space.total_bytes);
}

static void
test_missing_path_is_refused(void)
{
    struct filesystem_space space = {0};

    g_assert_false(filesystem_space_read("/nonexistent-path-for-a-test", &space));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/filesystem-free-space/root",    test_root_filesystem_is_measured},
    {"/filesystem-free-space/missing", test_missing_path_is_refused    },
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
