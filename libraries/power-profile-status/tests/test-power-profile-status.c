#include <glib.h>

#include "power-profile-status.h"

static void
test_profiles_cycle_in_order(void)
{
    const char *const all_profiles[] = {"performance", "balanced", "power-saver", NULL};

    g_assert_cmpstr(power_profile_following("power-saver", all_profiles), ==, "balanced");
    g_assert_cmpstr(power_profile_following("balanced", all_profiles), ==, "performance");
    g_assert_cmpstr(power_profile_following("performance", all_profiles), ==, "power-saver");
    g_assert_cmpstr(power_profile_following("unknown", all_profiles), ==, "power-saver");
}

/* Machines without a performance profile skip it. */
static void
test_missing_profiles_are_skipped(void)
{
    const char *const two_profiles[] = {"balanced", "power-saver", NULL};
    const char *const one_profile[]  = {"balanced", NULL};

    g_assert_cmpstr(power_profile_following("balanced", two_profiles), ==, "power-saver");
    g_assert_null(power_profile_following("balanced", one_profile));
    g_assert_null(power_profile_following("balanced", NULL));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/power-profile-status/order",   test_profiles_cycle_in_order     },
    {"/power-profile-status/missing", test_missing_profiles_are_skipped},
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
