#ifndef PROCESSOR_USAGE_SAMPLING_H
#define PROCESSOR_USAGE_SAMPLING_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Processor usage of the whole machine from the kernel's time counters in
 * /proc/stat: two samples taken some time apart give the share of that
 * time the processors were busy.
 */

struct processor_time_totals {
    uint64_t busy_ticks; // everything except idle and waiting for input/output
    uint64_t total_ticks;
};

/* Reads the "cpu" line of /proc/stat text; false if it is missing or malformed. */
bool processor_time_totals_parse(const char *statistics_text, struct processor_time_totals *totals);

bool processor_time_totals_read(struct processor_time_totals *totals);

/* Busy share between two samples, 0 to 100; -1 when no time passed or the counters went back. */
int processor_usage_percent(const struct processor_time_totals *earlier, const struct processor_time_totals *later);

#endif
