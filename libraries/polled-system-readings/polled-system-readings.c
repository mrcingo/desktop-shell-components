#include <glib.h>

#include "display-backlight-level.h"
#include "hardware-temperature-sensor.h"
#include "network-interface-traffic.h"
#include "polled-system-readings.h"
#include "processor-usage-sampling.h"

static const char hardware_monitor_directory[] = "/sys/class/hwmon";
static const char backlight_class_directory[]  = "/sys/class/backlight";
static const char net_class_directory[]        = "/sys/class/net";

enum {
    DEFAULT_INTERVAL_SECONDS = 2,
};

struct traffic_sample {
    struct network_interface_byte_counts counts;
    gint64                               taken_at_microseconds;
    bool                                 is_valid;
};

struct polled_system_readings {
    unsigned                                     enabled_readings;
    char                                        *disk_path;
    char                                        *temperature_input_path; // NULL without a sensor
    char                                        *interface_name;         // NULL without a network
    struct display_backlight                    *backlight;              // NULL without a backlight
    const struct polled_system_readings_handler *handler;
    void                                        *user_data;
    guint                                        timer_source;
    unsigned                                     tick_seconds;
    unsigned                                     ticks_per_reading[POLLED_SYSTEM_READING_KIND_COUNT];
    uint64_t                                     tick_count;

    struct processor_time_totals previous_processor_times;
    bool                         has_previous_processor_times;
    struct traffic_sample        previous_traffic;
    struct polled_system_values  values;
};

static bool
is_enabled(
    const struct polled_system_readings *readings,
    enum polled_system_reading           reading
)
{
    return (readings->enabled_readings & reading) != 0;
}

static void
read_processor_usage(
    struct polled_system_readings *readings
)
{
    struct processor_time_totals times = {0};

    if (!processor_time_totals_read(&times)) {
        return;
    }

    if (readings->has_previous_processor_times) {
        readings->values.processor_usage_percent = processor_usage_percent(&readings->previous_processor_times, &times);
    }

    readings->previous_processor_times     = times;
    readings->has_previous_processor_times = true;
}

static void
read_processor_temperature(
    struct polled_system_readings *readings
)
{
    if (readings->temperature_input_path == NULL) {
        return;
    }

    readings->values.has_processor_temperature = hardware_temperature_sensor_read_celsius(
        readings->temperature_input_path,
        &readings->values.processor_temperature_celsius
    );
}

static struct traffic_sample
take_traffic_sample(
    const char *interface_name
)
{
    struct traffic_sample sample = {.taken_at_microseconds = g_get_monotonic_time()};

    sample.is_valid = network_interface_byte_counts_read(net_class_directory, interface_name, &sample.counts);

    return sample;
}

static bool
both_samples_are_valid(
    const struct traffic_sample *earlier,
    const struct traffic_sample *later
)
{
    if (!earlier->is_valid) {
        return false;
    }

    return later->is_valid;
}

static void
compute_traffic_rates(
    struct polled_system_values *values,
    const struct traffic_sample *earlier,
    const struct traffic_sample *later
)
{
    gint64 elapsed_microseconds = later->taken_at_microseconds - earlier->taken_at_microseconds;

    values->download_bytes_per_second = network_interface_byte_rate(
        earlier->counts.received_bytes,
        later->counts.received_bytes,
        elapsed_microseconds
    );
    values->upload_bytes_per_second = network_interface_byte_rate(
        earlier->counts.transmitted_bytes,
        later->counts.transmitted_bytes,
        elapsed_microseconds
    );
    values->has_traffic = both_samples_are_valid(earlier, later);
}

static void
read_traffic(
    struct polled_system_readings *readings
)
{
    if (readings->interface_name == NULL) {
        return;
    }

    struct traffic_sample sample = take_traffic_sample(readings->interface_name);

    compute_traffic_rates(&readings->values, &readings->previous_traffic, &sample);

    readings->previous_traffic = sample;
}

static void
read_backlight(
    struct polled_system_readings *readings
)
{
    if (readings->backlight == NULL) {
        return;
    }

    readings->values.backlight_percent     = display_backlight_read_percent(readings->backlight);
    readings->values.backlight_is_writable = display_backlight_is_writable(readings->backlight);
}

static void
read_memory(
    struct polled_system_readings *readings
)
{
    readings->values.has_memory = memory_usage_read(&readings->values.memory);
}

static void
read_disk(
    struct polled_system_readings *readings
)
{
    readings->values.has_disk = filesystem_space_read(readings->disk_path, &readings->values.disk);
}

/* In the order of enum polled_system_reading. */
static void (*const readers[POLLED_SYSTEM_READING_KIND_COUNT])(struct polled_system_readings *readings) = {
    read_processor_usage,
    read_processor_temperature,
    read_memory,
    read_disk,
    read_traffic,
    read_backlight,
};

static bool
is_due(
    const struct polled_system_readings *readings,
    size_t                               kind_index
)
{
    if (!is_enabled(readings, 1u << kind_index)) {
        return false;
    }

    return readings->tick_count % readings->ticks_per_reading[kind_index] == 0;
}

/* Tick 0 reads everything enabled. */
static void
read_due(
    struct polled_system_readings *readings
)
{
    for (size_t i = 0; i < POLLED_SYSTEM_READING_KIND_COUNT; i++) {
        if (is_due(readings, i)) {
            readers[i](readings);
        }
    }
}

static gboolean
handle_timer(
    void *user_data
)
{
    struct polled_system_readings *readings = user_data;

    readings->tick_count++;

    read_due(readings);
    readings->handler->handle_values_read(readings->user_data, &readings->values);

    return G_SOURCE_CONTINUE;
}

static unsigned
interval_of(
    const struct polled_system_readings_options *options,
    size_t                                       kind_index
)
{
    if (options->interval_seconds[kind_index] == 0) {
        return DEFAULT_INTERVAL_SECONDS;
    }

    return options->interval_seconds[kind_index];
}

/* The shortest interval among the enabled readings. */
static unsigned
choose_tick_seconds(
    const struct polled_system_readings_options *options
)
{
    unsigned tick_seconds = G_MAXUINT;

    for (size_t i = 0; i < POLLED_SYSTEM_READING_KIND_COUNT; i++) {
        if ((options->readings & (1u << i)) != 0) {
            tick_seconds = MIN(tick_seconds, interval_of(options, i));
        }
    }

    if (tick_seconds == G_MAXUINT) {
        return DEFAULT_INTERVAL_SECONDS;
    }

    return tick_seconds;
}

static void
plan_ticks(
    struct polled_system_readings               *readings,
    const struct polled_system_readings_options *options
)
{
    readings->tick_seconds = choose_tick_seconds(options);

    for (size_t i = 0; i < POLLED_SYSTEM_READING_KIND_COUNT; i++) {
        readings->ticks_per_reading[i] = MAX(1, interval_of(options, i) / readings->tick_seconds);
    }
}

static void
find_devices(
    struct polled_system_readings *readings,
    const char                    *temperature_sensor_name
)
{
    if (is_enabled(readings, POLLED_SYSTEM_READING_TEMPERATURE)) {
        readings->temperature_input_path = hardware_temperature_sensor_find(
            hardware_monitor_directory,
            temperature_sensor_name
        );
    }

    if (is_enabled(readings, POLLED_SYSTEM_READING_BACKLIGHT)) {
        readings->backlight = display_backlight_find(backlight_class_directory);
    }
}

struct polled_system_readings *
polled_system_readings_create(
    const struct polled_system_readings_options *options,
    const struct polled_system_readings_handler *handler,
    void                                        *user_data
)
{
    struct polled_system_readings *readings = g_new0(struct polled_system_readings, 1);

    *readings = (struct polled_system_readings){
        .enabled_readings = options->readings,
        .disk_path        = g_strdup(options->disk_path),
        .handler          = handler,
        .user_data        = user_data,
        .values           = {.processor_usage_percent = -1, .backlight_percent = -1},
    };

    find_devices(readings, options->temperature_sensor_name);
    plan_ticks(readings, options);
    read_due(readings);

    readings->timer_source = g_timeout_add_seconds(readings->tick_seconds, handle_timer, readings);

    return readings;
}

static void
free_paths(
    struct polled_system_readings *readings
)
{
    g_free(readings->disk_path);
    g_free(readings->temperature_input_path);
    g_free(readings->interface_name);
}

void
polled_system_readings_destroy(
    struct polled_system_readings *readings
)
{
    if (readings == NULL) {
        return;
    }

    g_clear_handle_id(&readings->timer_source, g_source_remove);
    display_backlight_destroy(readings->backlight);
    free_paths(readings);
    g_free(readings);
}

const struct polled_system_values *
polled_system_readings_get_values(
    const struct polled_system_readings *readings
)
{
    return &readings->values;
}

static void
start_traffic_over(
    struct polled_system_readings *readings,
    const char                    *interface_name
)
{
    g_free(readings->interface_name);

    readings->interface_name     = g_strdup(interface_name);
    readings->previous_traffic   = (struct traffic_sample){0};
    readings->values.has_traffic = false;
}

void
polled_system_readings_set_network_interface(
    struct polled_system_readings *readings,
    const char                    *interface_name
)
{
    if (!is_enabled(readings, POLLED_SYSTEM_READING_TRAFFIC)) {
        return;
    }

    if (g_strcmp0(readings->interface_name, interface_name) == 0) {
        return;
    }

    start_traffic_over(readings, interface_name);
}

void
polled_system_readings_change_backlight(
    struct polled_system_readings *readings,
    int                            percent_points
)
{
    if (!readings->values.backlight_is_writable) {
        return;
    }

    display_backlight_set_percent(readings->backlight, readings->values.backlight_percent + percent_points);
    read_backlight(readings);
    readings->handler->handle_values_read(readings->user_data, &readings->values);
}
