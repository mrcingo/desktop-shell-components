#include <inttypes.h>
#include <stdio.h>

#include <glib.h>

#include "processor-usage-sampling.h"

enum {
    COUNTED_FIELDS = 8, // user nice system idle iowait irq softirq steal; guest time is already in user
    IDLE_FIELD     = 3,
    WAITING_FIELD  = 4,
};

static bool
parse_counters(
    const char *statistics_text,
    uint64_t    counters[COUNTED_FIELDS]
)
{
    int parsed_fields = sscanf(
        statistics_text,
        "cpu %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64,
        &counters[0],
        &counters[1],
        &counters[2],
        &counters[3],
        &counters[4],
        &counters[5],
        &counters[6],
        &counters[7]
    );

    return parsed_fields == COUNTED_FIELDS;
}

static uint64_t
sum_counters(
    const uint64_t counters[COUNTED_FIELDS]
)
{
    uint64_t sum = 0;

    for (size_t i = 0; i < COUNTED_FIELDS; i++) {
        sum += counters[i];
    }

    return sum;
}

bool
processor_time_totals_parse(
    const char                   *statistics_text,
    struct processor_time_totals *totals
)
{
    uint64_t counters[COUNTED_FIELDS] = {0};

    if (!parse_counters(statistics_text, counters)) {
        return false;
    }

    totals->total_ticks = sum_counters(counters);
    totals->busy_ticks  = totals->total_ticks - counters[IDLE_FIELD] - counters[WAITING_FIELD];

    return true;
}

bool
processor_time_totals_read(
    struct processor_time_totals *totals
)
{
    g_autofree char *statistics_text = NULL;

    if (!g_file_get_contents("/proc/stat", &statistics_text, NULL, NULL)) {
        return false;
    }

    return processor_time_totals_parse(statistics_text, totals);
}

int
processor_usage_percent(
    const struct processor_time_totals *earlier,
    const struct processor_time_totals *later
)
{
    if (later->total_ticks <= earlier->total_ticks) {
        return -1;
    }

    if (later->busy_ticks < earlier->busy_ticks) {
        return -1;
    }

    uint64_t busy_ticks  = later->busy_ticks - earlier->busy_ticks;
    uint64_t total_ticks = later->total_ticks - earlier->total_ticks;

    return (int)MIN(100, (busy_ticks * 100 + total_ticks / 2) / total_ticks);
}
