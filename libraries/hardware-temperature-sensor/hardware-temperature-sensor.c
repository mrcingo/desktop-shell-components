#include <glib.h>

#include "hardware-temperature-sensor.h"

enum {
    MILLIDEGREES_PER_DEGREE = 1000,
    LOWEST_CELSIUS          = -273,
    HIGHEST_CELSIUS         = 1000,
};

static bool
device_has_name(
    const char *device_directory,
    const char *sensor_name
)
{
    g_autofree char *name_path = g_build_filename(device_directory, "name", NULL);
    g_autofree char *name      = NULL;

    if (!g_file_get_contents(name_path, &name, NULL, NULL)) {
        return false;
    }

    return g_strcmp0(g_strstrip(name), sensor_name) == 0;
}

/* The device's first temperature input, if the device is the one asked for and has one. */
static char *
temperature_input_of(
    const char *device_directory,
    const char *sensor_name
)
{
    g_autofree char *input_path = g_build_filename(device_directory, "temp1_input", NULL);

    if (!device_has_name(device_directory, sensor_name)) {
        return NULL;
    }

    if (!g_file_test(input_path, G_FILE_TEST_EXISTS)) {
        return NULL;
    }

    return g_steal_pointer(&input_path);
}

static char *
temperature_input_of_entry(
    const char *hardware_monitor_directory,
    const char *entry_name,
    const char *sensor_name
)
{
    g_autofree char *device_directory = g_build_filename(hardware_monitor_directory, entry_name, NULL);

    return temperature_input_of(device_directory, sensor_name);
}

static char *
find_in_open_directory(
    GDir       *directory,
    const char *hardware_monitor_directory,
    const char *sensor_name
)
{
    const char *entry_name = NULL;

    while ((entry_name = g_dir_read_name(directory)) != NULL) {
        char *input_path = temperature_input_of_entry(hardware_monitor_directory, entry_name, sensor_name);

        if (input_path != NULL) {
            return input_path;
        }
    }

    return NULL;
}

char *
hardware_temperature_sensor_find(
    const char *hardware_monitor_directory,
    const char *sensor_name
)
{
    g_autoptr(GDir) directory = g_dir_open(hardware_monitor_directory, 0, NULL);

    if (directory == NULL) {
        return NULL;
    }

    return find_in_open_directory(directory, hardware_monitor_directory, sensor_name);
}

/* The kernel gives thousandths of a degree. */
static bool
parse_celsius(
    char *contents,
    int  *celsius
)
{
    gint64 lowest       = (gint64)LOWEST_CELSIUS * MILLIDEGREES_PER_DEGREE;
    gint64 highest      = (gint64)HIGHEST_CELSIUS * MILLIDEGREES_PER_DEGREE;
    gint64 millidegrees = 0;

    if (!g_ascii_string_to_signed(g_strstrip(contents), 10, lowest, highest, &millidegrees, NULL)) {
        return false;
    }

    *celsius = (int)(millidegrees / MILLIDEGREES_PER_DEGREE);

    return true;
}

bool
hardware_temperature_sensor_read_celsius(
    const char *input_path,
    int        *celsius
)
{
    g_autofree char *contents = NULL;

    if (!g_file_get_contents(input_path, &contents, NULL, NULL)) {
        return false;
    }

    return parse_celsius(contents, celsius);
}
