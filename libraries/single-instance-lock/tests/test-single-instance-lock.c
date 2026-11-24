#include <glib/gstdio.h>

#include "single-instance-lock.h"

static char *runtime_directory;

static void
use_private_runtime_directory(void)
{
    runtime_directory = g_dir_make_tmp("single-instance-lock-test-XXXXXX", NULL);

    g_setenv("XDG_RUNTIME_DIR", runtime_directory, TRUE);
    g_setenv("WAYLAND_DISPLAY", "wayland-test", TRUE);
}

static void
assert_lock_is_free(
    const char *instance_name
)
{
    struct single_instance_lock *lock = single_instance_lock_acquire(instance_name);

    g_assert_nonnull(lock);
    single_instance_lock_release(lock);
}

static void
test_second_copy_is_refused_until_release(void)
{
    struct single_instance_lock *first  = single_instance_lock_acquire("example-program");
    struct single_instance_lock *second = single_instance_lock_acquire("example-program");

    g_assert_nonnull(first);
    g_assert_null(second);

    single_instance_lock_release(first);
    assert_lock_is_free("example-program");
}

static void
test_different_programs_do_not_block_each_other(void)
{
    struct single_instance_lock *bar      = single_instance_lock_acquire("example-bar");
    struct single_instance_lock *switcher = single_instance_lock_acquire("example-switcher");

    g_assert_nonnull(bar);
    g_assert_nonnull(switcher);

    single_instance_lock_release(bar);
    single_instance_lock_release(switcher);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/single-instance-lock/second-copy-refused", test_second_copy_is_refused_until_release      },
    {"/single-instance-lock/independent-names",   test_different_programs_do_not_block_each_other},
};

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);
    use_private_runtime_directory();

    for (size_t i = 0; i < G_N_ELEMENTS(registered_tests); i++) {
        g_test_add_func(registered_tests[i].test_path, registered_tests[i].test_function);
    }

    return g_test_run();
}
