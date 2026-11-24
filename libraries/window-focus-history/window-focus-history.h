#ifndef WINDOW_FOCUS_HISTORY_H
#define WINDOW_FOCUS_HISTORY_H

#include <stdint.h>

#include <glib.h>

/*
 * Window ids ordered by when each window last had keyboard focus, most
 * recent first: the order an Alt+Tab switcher presents windows in.
 */

struct window_focus_history;

struct window_focus_history *window_focus_history_create(void);
void                         window_focus_history_destroy(struct window_focus_history *history);

void window_focus_history_record_focus(struct window_focus_history *history, int64_t window_id);
void window_focus_history_forget_window(struct window_focus_history *history, int64_t window_id);

/*
 * Brings the history in line with the windows that exist: windows missing
 * from window_ids_in_focus_order are dropped, and windows never seen being
 * focused are appended in that order.
 */
void window_focus_history_synchronize(struct window_focus_history *history, const GArray *window_ids_in_focus_order);

/* 0 for the most recently focused window, -1 for an unknown one. */
int window_focus_history_position_of(const struct window_focus_history *history, int64_t window_id);

/* A qsort-style comparison: more recently focused windows sort first. */
int window_focus_history_compare_recency(
    const struct window_focus_history *history,
    int64_t                            first_window_id,
    int64_t                            second_window_id
);

#endif
