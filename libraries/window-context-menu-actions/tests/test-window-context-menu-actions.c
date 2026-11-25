#include <unistd.h>

#include "window-context-menu-actions.h"

static char *empty_marks[] = {NULL};

static struct window_context_menu_target *
create_x11_target(void)
{
    const struct compositor_window window = {
        .container_id     = 7,
        .application_name = "xterm",
        .title            = "xterm",
        .workspace_name   = "1",
        .process_id       = getpid(),
        .is_x11_window    = true,
        .marks            = empty_marks,
    };

    return window_context_menu_target_create(&window, "/proc");
}

/* Signalling an X11 window's process would end the X server, so it is not offered. */
static void
test_x11_window_cannot_be_signalled(void)
{
    struct window_context_menu_target *target  = create_x11_target();
    GActionGroup                      *actions = window_context_menu_actions_create(target, NULL, NULL);

    g_assert_false(g_action_group_get_action_enabled(actions, "force-close"));
    g_assert_false(g_action_group_get_action_enabled(actions, "copy-process-id"));
    g_assert_true(g_action_group_get_action_enabled(actions, "copy-title"));
    g_object_unref(actions);
    window_context_menu_target_destroy(target);
}

/* Sticky only applies to floating windows. */
static void
test_window_states_are_shown(void)
{
    struct window_context_menu_target *target         = create_x11_target();
    GActionGroup                      *actions        = window_context_menu_actions_create(target, NULL, NULL);
    GVariant                          *floating_state = g_action_group_get_action_state(actions, "toggle-floating");

    g_assert_false(g_variant_get_boolean(floating_state) || g_action_group_get_action_enabled(actions, "toggle-sticky"));
    g_variant_unref(floating_state);
    g_object_unref(actions);
    window_context_menu_target_destroy(target);
}

static void
test_workspace_numbers_are_checked(void)
{
    g_assert_true(window_context_menu_is_workspace_number("3"));
    g_assert_false(window_context_menu_is_workspace_number("0"));
    g_assert_false(window_context_menu_is_workspace_number("1; exec rm"));
    g_assert_false(window_context_menu_is_workspace_number(""));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/window-context-menu-actions/x11",        test_x11_window_cannot_be_signalled},
    {"/window-context-menu-actions/states",     test_window_states_are_shown       },
    {"/window-context-menu-actions/workspaces", test_workspace_numbers_are_checked },
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
