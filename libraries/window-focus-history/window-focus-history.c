#include "window-focus-history.h"

struct window_focus_history {
    GArray *window_ids_most_recent_first; // int64_t
};

struct window_focus_history *
window_focus_history_create(void)
{
    struct window_focus_history *history = g_new0(struct window_focus_history, 1);

    history->window_ids_most_recent_first = g_array_new(FALSE, FALSE, sizeof(int64_t));

    return history;
}

void
window_focus_history_destroy(
    struct window_focus_history *history
)
{
    if (history == NULL) {
        return;
    }

    g_array_unref(history->window_ids_most_recent_first);
    g_free(history);
}

static int
find_position(
    const GArray *window_ids,
    int64_t       window_id
)
{
    for (guint i = 0; i < window_ids->len; i++) {
        if (g_array_index(window_ids, int64_t, i) == window_id) {
            return (int)i;
        }
    }

    return -1;
}

int
window_focus_history_position_of(
    const struct window_focus_history *history,
    int64_t                            window_id
)
{
    return find_position(history->window_ids_most_recent_first, window_id);
}

void
window_focus_history_forget_window(
    struct window_focus_history *history,
    int64_t                      window_id
)
{
    int position = window_focus_history_position_of(history, window_id);

    if (position < 0) {
        return;
    }

    g_array_remove_index(history->window_ids_most_recent_first, (guint)position);
}

void
window_focus_history_record_focus(
    struct window_focus_history *history,
    int64_t                      window_id
)
{
    window_focus_history_forget_window(history, window_id);
    g_array_prepend_val(history->window_ids_most_recent_first, window_id);
}

static void
append_window_ids_also_in(
    GArray       *destination,
    const GArray *source,
    const GArray *filter
)
{
    for (guint i = 0; i < source->len; i++) {
        int64_t window_id = g_array_index(source, int64_t, i);

        if (find_position(filter, window_id) >= 0) {
            g_array_append_val(destination, window_id);
        }
    }
}

static void
append_window_ids_not_yet_in(
    GArray       *destination,
    const GArray *source
)
{
    for (guint i = 0; i < source->len; i++) {
        int64_t window_id = g_array_index(source, int64_t, i);

        if (find_position(destination, window_id) < 0) {
            g_array_append_val(destination, window_id);
        }
    }
}

void
window_focus_history_synchronize(
    struct window_focus_history *history,
    const GArray                *window_ids_in_focus_order
)
{
    GArray *previous     = history->window_ids_most_recent_first;
    GArray *synchronized = g_array_new(FALSE, FALSE, sizeof(int64_t));

    append_window_ids_also_in(synchronized, previous, window_ids_in_focus_order);
    append_window_ids_not_yet_in(synchronized, window_ids_in_focus_order);
    g_array_unref(previous);

    history->window_ids_most_recent_first = synchronized;
}

/* Unknown windows sort after every known one. */
static int
sortable_position(
    const struct window_focus_history *history,
    int64_t                            window_id
)
{
    int position = window_focus_history_position_of(history, window_id);

    if (position < 0) {
        return G_MAXINT;
    }

    return position;
}

int
window_focus_history_compare_recency(
    const struct window_focus_history *history,
    int64_t                            first_window_id,
    int64_t                            second_window_id
)
{
    int first  = sortable_position(history, first_window_id);
    int second = sortable_position(history, second_window_id);

    return (first > second) - (first < second);
}
