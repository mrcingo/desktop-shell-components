#include <glib.h>

#include "polled-system-readings.h"

static int values_read_count;

static void
count_values_read(
    void                              *user_data,
    const struct polled_system_values *values
)
{
    values_read_count++;
}

static const struct polled_system_readings_handler handler = {
    .handle_values_read = count_values_read,
};

static gboolean
stop_loop(
    void *user_data
)
{
    g_main_loop_quit(user_data);

    return G_SOURCE_REMOVE;
}

static void
run_main_loop_for(
    guint milliseconds
)
{
    g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);

    g_timeout_add(milliseconds, stop_loop, loop);
    g_main_loop_run(loop);
}

static void
assert_values_were_read(
    const struct polled_system_values *values
)
{
    g_assert_cmpint(values_read_count, >=, 2);
    g_assert_cmpint(values->processor_usage_percent, >=, 0);
    g_assert_true(values->has_memory);
    g_assert_true(values->has_disk);
    g_assert_true(values->has_traffic);
    g_assert_false(values->has_processor_temperature);
}

/* Two rounds give a processor usage and a traffic rate for the loopback interface. */
static void
test_values_are_read_on_the_timer(void)
{
    const struct polled_system_readings_options options = {
        .readings                = POLLED_SYSTEM_READING_PROCESSOR | POLLED_SYSTEM_READING_MEMORY | POLLED_SYSTEM_READING_DISK |
                                   POLLED_SYSTEM_READING_TRAFFIC,
        .interval_seconds        = {1, 1, 1, 1, 1, 1},
        .disk_path               = "/",
        .temperature_sensor_name = "no-such-sensor",
    };

    struct polled_system_readings *readings = polled_system_readings_create(&options, &handler, NULL);

    polled_system_readings_set_network_interface(readings, "lo");
    run_main_loop_for(2500);
    assert_values_were_read(polled_system_readings_get_values(readings));
    polled_system_readings_destroy(readings);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/polled-system-readings/timer", test_values_are_read_on_the_timer},
};

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);

    for (size_t i = 0; i < G_N_ELEMENTS(registered_tests); i++) {
        g_test_add_func(registered_tests[i].test_path, registered_tests[i].test_function);
    }

    return g_test_run();
}
