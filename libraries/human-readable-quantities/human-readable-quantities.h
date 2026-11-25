#ifndef HUMAN_READABLE_QUANTITIES_H
#define HUMAN_READABLE_QUANTITIES_H

#include <stdint.h>

/*
 * Short texts for quantities shown in narrow places such as a status bar.
 * Free every result with g_free().
 */

/* Binary units with one decimal below ten: "512B", "1.2K", "830K", "15G". */
char *human_readable_byte_size(uint64_t bytes);

/* Hours and minutes: "2h 15m", "45m", "0m". Negative durations read as 0m. */
char *human_readable_duration(int64_t seconds);

#endif
