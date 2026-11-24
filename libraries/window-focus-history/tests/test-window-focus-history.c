#include "window-focus-history.h"

struct expected_position {
    int64_t window_id;
    int     position;
};

static struct window_focus_history *
create_history_with_focus_sequence(
    const int64_t *focused_window_ids,
    guint          focus_count
)
{
    struct window_focus_history *history = window_focus_history_create();

    for (guint i = 0; i < focus_count; i++) {
        window_focus_history_record_focus(history, focused_window_ids[i]);
    }

    return history;
}

static void
synchronize_with_window_ids(
    struct window_focus_history *history,
    const int64_t               *existing_window_ids,
    guint                        existing_count
)
{
    GArray *focus_order = g_array_new(FALSE, FALSE, sizeof(int64_t));

    g_array_append_vals(focus_order, existing_window_ids, existing_count);
    window_focus_history_synchronize(history, focus_order);
    g_array_unref(focus_order);
}

static void
assert_positions(
    const struct window_focus_history *history,
    const struct expected_position    *expected_positions,
    guint                              expected_count
)
{
    for (guint i = 0; i < expected_count; i++) {
        g_assert_cmpint(
            window_focus_history_position_of(history, expected_positions[i].window_id),
            ==,
            expected_positions[i].position
        );
    }
}

static void
test_focus_moves_window_to_front(void)
{
    static const int64_t                  focus_sequence[] = {1, 2, 1};
    static const struct expected_position expected[]       = {
        {1, 0 },
        {2, 1 },
        {3, -1}
    };

    struct window_focus_history *history = create_history_with_focus_sequence(focus_sequence, G_N_ELEMENTS(focus_sequence));

    assert_positions(history, expected, G_N_ELEMENTS(expected));
    g_assert_cmpint(window_focus_history_compare_recency(history, 1, 2), <, 0);
    g_assert_cmpint(window_focus_history_compare_recency(history, 3, 2), >, 0);
    window_focus_history_destroy(history);
}

static void
test_forgotten_window_leaves_the_history(void)
{
    static const int64_t focus_sequence[] = {1, 2, 1};

    struct window_focus_history *history = create_history_with_focus_sequence(focus_sequence, G_N_ELEMENTS(focus_sequence));

    window_focus_history_forget_window(history, 1);
    g_assert_cmpint(window_focus_history_position_of(history, 2), ==, 0);
    window_focus_history_destroy(history);
}

/* Window 5 closed meanwhile; window 7 was never seen being focused. */
static void
test_synchronize_keeps_known_order(void)
{
    static const int64_t                  focus_sequence[] = {5, 2, 1};
    static const int64_t                  existing[]       = {2, 1, 7};
    static const struct expected_position expected[]       = {
        {1, 0 },
        {2, 1 },
        {7, 2 },
        {5, -1}
    };

    struct window_focus_history *history = create_history_with_focus_sequence(focus_sequence, G_N_ELEMENTS(focus_sequence));

    synchronize_with_window_ids(history, existing, G_N_ELEMENTS(existing));
    assert_positions(history, expected, G_N_ELEMENTS(expected));
    window_focus_history_destroy(history);
}

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/window-focus-history/focus-moves-to-front", test_focus_moves_window_to_front);
    g_test_add_func("/window-focus-history/forget-window", test_forgotten_window_leaves_the_history);
    g_test_add_func("/window-focus-history/synchronize", test_synchronize_keeps_known_order);

    return g_test_run();
}
