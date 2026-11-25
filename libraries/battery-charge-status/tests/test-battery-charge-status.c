#include <glib.h>

#include "battery-charge-status.h"

static GVariant *
build_discharging_battery(void)
{
    g_auto(GVariantBuilder) builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);

    g_variant_builder_add(&builder, "{sv}", "IsPresent", g_variant_new_boolean(TRUE));
    g_variant_builder_add(&builder, "{sv}", "Percentage", g_variant_new_double(31.6));
    g_variant_builder_add(&builder, "{sv}", "State", g_variant_new_uint32(2));
    g_variant_builder_add(&builder, "{sv}", "TimeToEmpty", g_variant_new_int64(8100));
    g_variant_builder_add(&builder, "{sv}", "TimeToFull", g_variant_new_int64(-3));

    return g_variant_ref_sink(g_variant_builder_end(&builder));
}

/* The percentage is rounded and a negative time reads as unknown. */
static void
assert_discharging_battery(
    const struct battery_charge_status *status
)
{
    g_assert_true(status->is_present);
    g_assert_cmpint(status->percent, ==, 32);
    g_assert_cmpint(status->state, ==, BATTERY_CHARGE_STATE_DISCHARGING);
    g_assert_cmpint(status->seconds_until_empty, ==, 8100);
    g_assert_cmpint(status->seconds_until_full, ==, 0);
}

static void
test_battery_is_read(void)
{
    g_autoptr(GVariant) properties = build_discharging_battery();

    struct battery_charge_status status = {0};

    battery_charge_status_parse(properties, &status);
    assert_discharging_battery(&status);
}

static void
test_missing_properties_mean_no_battery(void)
{
    struct battery_charge_status status = {.is_present = true, .percent = 50};

    battery_charge_status_parse(NULL, &status);

    g_assert_false(status.is_present);
    g_assert_cmpint(status.state, ==, BATTERY_CHARGE_STATE_UNKNOWN);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/battery-charge-status/read",    test_battery_is_read                   },
    {"/battery-charge-status/missing", test_missing_properties_mean_no_battery},
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
