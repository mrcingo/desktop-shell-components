#include <glib.h>

#include "system-service-readings.h"

static void
ignore_values(
    void                               *user_data,
    const struct system_service_values *values
)
{}

static const struct system_service_readings_handler handler = {
    .handle_values_changed = ignore_values,
};

static void
assert_every_service_absent(
    const struct system_service_values *values
)
{
    g_assert_false(values->network.service_is_running);
    g_assert_false(values->bluetooth.adapter_is_present);
    g_assert_null(values->power_profile.active_profile);
    g_assert_false(values->battery.is_present);
}

/*
 * Without a system bus (DBUS_SYSTEM_BUS_ADDRESS pointing nowhere) every
 * service reads as absent and actions do nothing.
 */
static void
test_missing_system_bus_reads_as_absent(void)
{
    g_setenv("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent-system-bus-socket", TRUE);

    struct system_service_readings *readings = system_service_readings_create(
        SYSTEM_SERVICE_READING_NETWORK | SYSTEM_SERVICE_READING_BLUETOOTH | SYSTEM_SERVICE_READING_POWER_PROFILE |
            SYSTEM_SERVICE_READING_BATTERY,
        &handler,
        NULL
    );

    assert_every_service_absent(system_service_readings_get_values(readings));
    system_service_readings_toggle_bluetooth(readings);
    system_service_readings_switch_power_profile(readings);
    system_service_readings_destroy(readings);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/system-service-readings/missing-bus", test_missing_system_bus_reads_as_absent},
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
