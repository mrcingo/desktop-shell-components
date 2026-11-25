#ifndef POLLED_SYSTEM_READINGS_H
#define POLLED_SYSTEM_READINGS_H

#include <stdbool.h>
#include <stdint.h>

#include "filesystem-free-space.h"
#include "memory-usage-reading.h"

/*
 * Values the kernel offers no change notification for: processor use and
 * temperature, memory, free disk space, network traffic rates and the
 * backlight level. One repeating timer ticks at the shortest interval asked
 * for; each reading is taken on the ticks that match its own interval.
 * Only the readings asked for are taken.
 */

enum polled_system_reading {
    POLLED_SYSTEM_READING_PROCESSOR   = 1 << 0,
    POLLED_SYSTEM_READING_TEMPERATURE = 1 << 1,
    POLLED_SYSTEM_READING_MEMORY      = 1 << 2,
    POLLED_SYSTEM_READING_DISK        = 1 << 3,
    POLLED_SYSTEM_READING_TRAFFIC     = 1 << 4,
    POLLED_SYSTEM_READING_BACKLIGHT   = 1 << 5,
};

enum {
    POLLED_SYSTEM_READING_KIND_COUNT = 6,
};

struct polled_system_values {
    int  processor_usage_percent; // -1 until two samples were taken
    int  processor_temperature_celsius;
    bool has_processor_temperature;

    struct memory_usage memory;
    bool                has_memory;

    struct filesystem_space disk;
    bool                    has_disk;

    uint64_t download_bytes_per_second;
    uint64_t upload_bytes_per_second;
    bool     has_traffic; // false until two samples of the same interface were taken

    int  backlight_percent; // -1 without a backlight
    bool backlight_is_writable;
};

struct polled_system_readings_options {
    unsigned readings; // enum polled_system_reading flags
    /* Per reading, in the order of enum polled_system_reading; 0 means 2 seconds. */
    unsigned    interval_seconds[POLLED_SYSTEM_READING_KIND_COUNT];
    const char *disk_path;               // the filesystem measured, such as "/"
    const char *temperature_sensor_name; // a hwmon driver name, such as "k10temp"
};

struct polled_system_readings_handler {
    /* After every tick on which something was read. */
    void (*handle_values_read)(void *user_data, const struct polled_system_values *values);
};

/* Takes the first readings at once; the strings in options are copied. */
struct polled_system_readings *polled_system_readings_create(
    const struct polled_system_readings_options *options,
    const struct polled_system_readings_handler *handler,
    void                                        *user_data
);

void polled_system_readings_destroy(struct polled_system_readings *readings);

const struct polled_system_values *polled_system_readings_get_values(const struct polled_system_readings *readings);

/* The interface whose traffic is measured, or NULL for none; the rate starts over. */
void polled_system_readings_set_network_interface(struct polled_system_readings *readings, const char *interface_name);

/* Changes the backlight by percent_points if it is writable, then reads it again. */
void polled_system_readings_change_backlight(struct polled_system_readings *readings, int percent_points);

#endif
