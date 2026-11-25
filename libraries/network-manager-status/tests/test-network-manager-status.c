#include <glib.h>

#include "network-manager-status.h"

static void
test_connection_types_map_to_kinds(void)
{
    g_assert_cmpint(network_connection_kind_from_type("802-11-wireless"), ==, NETWORK_CONNECTION_KIND_WIRELESS);
    g_assert_cmpint(network_connection_kind_from_type("802-3-ethernet"), ==, NETWORK_CONNECTION_KIND_WIRED);
    g_assert_cmpint(network_connection_kind_from_type("gsm"), ==, NETWORK_CONNECTION_KIND_OTHER);
    g_assert_cmpint(network_connection_kind_from_type(NULL), ==, NETWORK_CONNECTION_KIND_OTHER);
}

static void
test_connectivity_states_are_checked(void)
{
    g_assert_cmpint(network_connectivity_from_state(4), ==, NETWORK_CONNECTIVITY_FULL);
    g_assert_cmpint(network_connectivity_from_state(1), ==, NETWORK_CONNECTIVITY_NONE);
    g_assert_cmpint(network_connectivity_from_state(77), ==, NETWORK_CONNECTIVITY_UNKNOWN);
}

/* Signals from other access points, scanned in the background, change nothing shown. */
static void
test_only_relevant_objects_matter(void)
{
    struct network_manager_status status = {
        .primary_connection_path = "/org/freedesktop/NetworkManager/ActiveConnection/3",
        .access_point_path       = "/org/freedesktop/NetworkManager/AccessPoint/7",
    };

    g_assert_true(network_manager_status_concerns_object(&status, "/org/freedesktop/NetworkManager"));
    g_assert_true(network_manager_status_concerns_object(&status, "/org/freedesktop/NetworkManager/AccessPoint/7"));
    g_assert_false(network_manager_status_concerns_object(&status, "/org/freedesktop/NetworkManager/AccessPoint/8"));
    g_assert_true(network_manager_status_concerns_object(&status, NULL));
}

/* Reading without NetworkManager (a bus with no such service) gives an empty status, not a crash. */
static void
test_cleared_status_is_empty(void)
{
    struct network_manager_status status = {.connection_name = g_strdup("Home"), .wifi_signal_percent = 50};

    network_manager_status_clear(&status);

    g_assert_null(status.connection_name);
    g_assert_cmpint(status.wifi_signal_percent, ==, -1);
    g_assert_false(status.service_is_running);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/network-manager-status/kinds",        test_connection_types_map_to_kinds  },
    {"/network-manager-status/connectivity", test_connectivity_states_are_checked},
    {"/network-manager-status/relevance",    test_only_relevant_objects_matter   },
    {"/network-manager-status/clear",        test_cleared_status_is_empty        },
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
