#ifndef MEMORY_USAGE_READING_H
#define MEMORY_USAGE_READING_H

#include <stdbool.h>
#include <stdint.h>

/* Memory and swap use from /proc/meminfo, in kibibytes. */

struct memory_usage {
    uint64_t total_kibibytes;
    uint64_t available_kibibytes; // what programs can still get without swapping
    uint64_t swap_total_kibibytes;
    uint64_t swap_free_kibibytes;
};

/* False unless MemTotal and MemAvailable are present; missing swap lines read as no swap. */
bool memory_usage_parse(const char *memory_information_text, struct memory_usage *usage);

bool memory_usage_read(struct memory_usage *usage);

#endif
