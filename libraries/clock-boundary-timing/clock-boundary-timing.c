#include <string.h>

#include "clock-boundary-timing.h"

static const char second_conversions[] = "STrsXc+";

guint
clock_milliseconds_until_next_boundary(
    gint64 now_microseconds,
    guint  period_seconds
)
{
    gint64 period_microseconds = (gint64)MAX(period_seconds, 1) * G_USEC_PER_SEC;
    gint64 into_period         = ((now_microseconds % period_microseconds) + period_microseconds) % period_microseconds;
    gint64 remaining           = period_microseconds - into_period;

    return (guint)MAX(1, (remaining + 999) / 1000);
}

static bool
is_seconds_conversion(
    char conversion
)
{
    if (conversion == '\0') {
        return false;
    }

    return strchr(second_conversions, conversion) != NULL;
}

/* The next "%" after the conversion at percent, or NULL; a "%" at the very end has no conversion. */
static const char *
next_conversion(
    const char *percent
)
{
    if (percent[1] == '\0') {
        return NULL;
    }

    return strchr(percent + 2, '%');
}

/* "%%S" is a percent sign followed by S, not seconds. */
bool
clock_format_shows_seconds(
    const char *time_format
)
{
    for (const char *percent = strchr(time_format, '%'); percent != NULL; percent = next_conversion(percent)) {
        if (is_seconds_conversion(percent[1])) {
            return true;
        }
    }

    return false;
}
