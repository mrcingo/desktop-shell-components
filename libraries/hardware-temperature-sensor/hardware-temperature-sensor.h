#ifndef HARDWARE_TEMPERATURE_SENSOR_H
#define HARDWARE_TEMPERATURE_SENSOR_H

#include <stdbool.h>

/*
 * Temperatures from the kernel's hardware monitoring devices. Devices are
 * found by their driver name (such as k10temp for AMD processors), never by
 * their number, which changes between boots.
 */

/*
 * The first temperature input (temp1_input) of the device named sensor_name
 * under hardware_monitor_directory (normally /sys/class/hwmon), or NULL.
 * Free the result with g_free().
 */
char *hardware_temperature_sensor_find(const char *hardware_monitor_directory, const char *sensor_name);

/* Whole degrees Celsius; false if the input cannot be read. */
bool hardware_temperature_sensor_read_celsius(const char *input_path, int *celsius);

#endif
