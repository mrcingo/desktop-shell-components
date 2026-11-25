#include <inttypes.h>
#include <stdbool.h>

#include <glib.h>

#include "human-readable-quantities.h"

static const char unit_letters[] = "KMGTPE";

enum {
    BYTES_PER_KIBIBYTE = 1024,
    SECONDS_PER_MINUTE = 60,
    MINUTES_PER_HOUR   = 60,
};

/* The last unit letter takes any size, however large. */
static bool
can_scale_further(
    double value,
    size_t unit_index
)
{
    if (value < BYTES_PER_KIBIBYTE) {
        return false;
    }

    return unit_index + 2 < sizeof(unit_letters);
}

/* Divides until the value is below 1024 and returns the index of its unit letter. */
static size_t
scale_to_unit(
    double *value
)
{
    size_t unit_index = 0;

    for (*value /= BYTES_PER_KIBIBYTE; can_scale_further(*value, unit_index); unit_index++) {
        *value /= BYTES_PER_KIBIBYTE;
    }

    return unit_index;
}

char *
human_readable_byte_size(
    uint64_t bytes
)
{
    double value = (double)bytes;

    if (bytes < BYTES_PER_KIBIBYTE) {
        return g_strdup_printf("%" PRIu64 "B", bytes);
    }

    size_t unit_index = scale_to_unit(&value);

    if (value < 10) {
        return g_strdup_printf("%.1f%c", value, unit_letters[unit_index]);
    }

    return g_strdup_printf("%.0f%c", value, unit_letters[unit_index]);
}

char *
human_readable_duration(
    int64_t seconds
)
{
    int64_t minutes = MAX(seconds, 0) / SECONDS_PER_MINUTE;
    int64_t hours   = minutes / MINUTES_PER_HOUR;

    if (hours == 0) {
        return g_strdup_printf("%" PRId64 "m", minutes);
    }

    return g_strdup_printf("%" PRId64 "h %02" PRId64 "m", hours, minutes % MINUTES_PER_HOUR);
}
