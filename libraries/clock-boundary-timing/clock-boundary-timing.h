#ifndef CLOCK_BOUNDARY_TIMING_H
#define CLOCK_BOUNDARY_TIMING_H

#include <stdbool.h>

#include <glib.h>

/*
 * When a clock display must change: on the next whole minute, or the next
 * whole second when its format shows seconds. Waking up exactly then, rather
 * than on a short repeating timer, keeps an idle clock asleep.
 */

/* Milliseconds from now_microseconds (wall-clock, as g_get_real_time()) to the next multiple of period_seconds; at least 1. */
guint clock_milliseconds_until_next_boundary(gint64 now_microseconds, guint period_seconds);

/* Whether a strftime-style format shows seconds (%S, %T, %r, %s, %X, %c, %+). */
bool clock_format_shows_seconds(const char *time_format);

#endif
