#include <glib.h>
#include <glib/gstdio.h>

#include "hardware-temperature-sensor.h"

static void
write_file(
    const char *directory,
    const char *file_name,
    const char *contents
)
{
    g_autofree char *path = g_build_filename(directory, file_name, NULL);

    g_assert_true(g_file_set_contents(path, contents, -1, NULL));
}

static void
create_device(
    const char *root,
    const char *device_name,
    const char *driver_name
)
{
    g_autofree char *device = g_build_filename(root, device_name, NULL);

    g_mkdir(device, 0700);
    write_file(device, "name", driver_name);
}

/* hwmon0 is another driver, hwmon1 the processor's: the number must not matter. */
static char *
create_fake_hardware_monitors(void)
{
    char *root = g_dir_make_tmp("hwmon-XXXXXX", NULL);

    g_autofree char *processor = g_build_filename(root, "hwmon1", NULL);

    create_device(root, "hwmon0", "nvme\n");
    create_device(root, "hwmon1", "k10temp\n");
    write_file(processor, "temp1_input", "54875\n");

    return root;
}

static const char *const fake_files[] = {"hwmon0/name", "hwmon1/name", "hwmon1/temp1_input", "hwmon0", "hwmon1", ""};

/* Only the files created above, deepest first; g_remove() also removes empty directories. */
static void
remove_fake_hardware_monitors(
    const char *root
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(fake_files); i++) {
        g_autofree char *path = g_build_filename(root, fake_files[i], NULL);

        g_assert_cmpint(g_remove(path), ==, 0);
    }
}

static void
test_sensor_is_found_by_name(void)
{
    g_autofree char *root       = create_fake_hardware_monitors();
    g_autofree char *input_path = hardware_temperature_sensor_find(root, "k10temp");

    int celsius = 0;

    g_assert_true(hardware_temperature_sensor_read_celsius(input_path, &celsius));
    g_assert_cmpint(celsius, ==, 54);
    g_assert_null(hardware_temperature_sensor_find(root, "coretemp"));
    remove_fake_hardware_monitors(root);
}

static void
test_missing_directory_finds_nothing(void)
{
    int celsius = 0;

    g_assert_null(hardware_temperature_sensor_find("/nonexistent-hardware-monitors", "k10temp"));
    g_assert_false(hardware_temperature_sensor_read_celsius("/nonexistent-hardware-monitors/temp1_input", &celsius));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/hardware-temperature-sensor/by-name", test_sensor_is_found_by_name        },
    {"/hardware-temperature-sensor/missing", test_missing_directory_finds_nothing},
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
