#include <glib.h>

#include "network-interface-traffic.h"

static void
test_rates_are_computed(void)
{
    g_assert_cmpuint(network_interface_byte_rate(1000, 3000, 2 * G_USEC_PER_SEC), ==, 1000);
    g_assert_cmpuint(network_interface_byte_rate(3000, 1000, G_USEC_PER_SEC), ==, 0);
    g_assert_cmpuint(network_interface_byte_rate(1000, 3000, 0), ==, 0);
}

static void
test_unsafe_names_are_refused(void)
{
    struct network_interface_byte_counts counts = {0};

    g_assert_false(network_interface_byte_counts_read("/sys/class/net", "", &counts));
    g_assert_false(network_interface_byte_counts_read("/sys/class/net", "..", &counts));
    g_assert_false(network_interface_byte_counts_read("/sys/class/net", "lo/../lo", &counts));
}

/* Every Linux system has a loopback interface. */
static void
test_loopback_counters_are_readable(void)
{
    struct network_interface_byte_counts counts = {0};

    g_assert_true(network_interface_byte_counts_read("/sys/class/net", "lo", &counts));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/network-interface-traffic/rates",    test_rates_are_computed            },
    {"/network-interface-traffic/unsafe",   test_unsafe_names_are_refused      },
    {"/network-interface-traffic/loopback", test_loopback_counters_are_readable},
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
