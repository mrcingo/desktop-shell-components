#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <glib.h>

#include "process-signal-handle.h"

static void
test_refused_processes(void)
{
    g_autoptr(GError) error = NULL;

    g_assert_null(process_signal_handle_open(1, &error));
    g_clear_error(&error);
    g_assert_null(process_signal_handle_open(0, &error));
    g_clear_error(&error);
    g_assert_null(process_signal_handle_open(getpid(), &error));
}

static pid_t
start_sleeping_child(void)
{
    pid_t child_process_id = fork();

    if (child_process_id == 0) {
        sleep(60);
        _exit(0);
    }

    return child_process_id;
}

/* A child that would sleep for a minute is ended at once. */
static void
test_child_is_killed(void)
{
    pid_t                         child_process_id = start_sleeping_child();
    struct process_signal_handle *handle           = process_signal_handle_open(child_process_id, NULL);
    int                           status           = 0;

    g_assert_true(process_signal_handle_send(handle, SIGKILL, NULL));
    g_assert_cmpint(waitpid(child_process_id, &status, 0), ==, child_process_id);
    g_assert_true(WIFSIGNALED(status));
    process_signal_handle_close(handle);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/process-signal-handle/refused", test_refused_processes},
    {"/process-signal-handle/kill",    test_child_is_killed  },
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
