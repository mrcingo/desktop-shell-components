#include <unistd.h>

#include <glib.h>

#include "window-context-menu-target.h"

static char *empty_marks[] = {NULL};

/* The test process itself stands in for the window's process: it is the only one surely there. */
static struct compositor_window
window_of_process(
    int  process_id,
    bool is_x11_window
)
{
    return (struct compositor_window){
        .container_id     = 7,
        .application_name = "foot",
        .title            = "~",
        .workspace_name   = "1",
        .process_id       = process_id,
        .is_x11_window    = is_x11_window,
        .marks            = empty_marks,
    };
}

/* The own process is never signalled, but its details are read. */
static void
test_own_process_is_described(void)
{
    struct compositor_window           window = window_of_process(getpid(), false);
    struct window_context_menu_target *target = window_context_menu_target_create(&window, "/proc");

    g_autofree char *text = window_context_menu_target_describe_process(target);

    g_assert_nonnull(target->process);
    g_assert_null(target->signal_handle);
    g_assert_true(g_str_has_prefix(text, "foot  PID "));
    window_context_menu_target_destroy(target);
}

static void
test_x11_window_has_no_process(void)
{
    struct compositor_window           window = window_of_process(getpid(), true);
    struct window_context_menu_target *target = window_context_menu_target_create(&window, "/proc");

    g_autofree char *text = window_context_menu_target_describe_process(target);

    g_assert_null(target->process);
    g_assert_cmpstr(text, ==, "foot  X11 window");
    window_context_menu_target_destroy(target);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/window-context-menu-target/own-process", test_own_process_is_described },
    {"/window-context-menu-target/x11",         test_x11_window_has_no_process},
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
