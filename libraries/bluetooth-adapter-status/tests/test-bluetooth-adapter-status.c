#include <glib.h>

#include "bluetooth-adapter-status.h"

static void
add_object(
    GVariantBuilder *objects,
    const char      *object_path,
    const char      *interface_name,
    const char      *property_name,
    gboolean         property_value
)
{
    g_auto(GVariantBuilder) properties = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
    g_auto(GVariantBuilder) interfaces = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a{sa{sv}}"));

    g_variant_builder_add(&properties, "{sv}", property_name, g_variant_new_boolean(property_value));
    g_variant_builder_add(&interfaces, "{s@a{sv}}", interface_name, g_variant_builder_end(&properties));
    g_variant_builder_add(objects, "{o@a{sa{sv}}}", object_path, g_variant_builder_end(&interfaces));
}

static GVariant *
build_example_reply(void)
{
    g_auto(GVariantBuilder) objects = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a{oa{sa{sv}}}"));

    add_object(&objects, "/org/bluez/hci0", "org.bluez.Adapter1", "Powered", TRUE);
    add_object(&objects, "/org/bluez/hci0/dev_1", "org.bluez.Device1", "Connected", TRUE);
    add_object(&objects, "/org/bluez/hci0/dev_2", "org.bluez.Device1", "Connected", FALSE);
    add_object(&objects, "/org/bluez/hci0/dev_3", "org.bluez.Device1", "Connected", TRUE);

    return g_variant_ref_sink(g_variant_new("(@a{oa{sa{sv}}})", g_variant_builder_end(&objects)));
}

static void
assert_powered_with_two_devices(
    const struct bluetooth_adapter_status *status
)
{
    g_assert_true(status->adapter_is_present);
    g_assert_true(status->is_powered);
    g_assert_cmpint(status->connected_device_count, ==, 2);
    g_assert_cmpstr(status->adapter_path, ==, "/org/bluez/hci0");
}

static void
test_adapter_and_devices_are_counted(void)
{
    g_autoptr(GVariant) reply = build_example_reply();

    struct bluetooth_adapter_status status = {0};

    bluetooth_adapter_status_parse(reply, &status);
    assert_powered_with_two_devices(&status);
    bluetooth_adapter_status_clear(&status);
}

static void
test_missing_or_wrong_reply_means_no_adapter(void)
{
    g_autoptr(GVariant) wrong_reply = g_variant_ref_sink(g_variant_new("(s)", "text"));

    struct bluetooth_adapter_status status = {0};

    bluetooth_adapter_status_parse(NULL, &status);
    g_assert_false(status.adapter_is_present);
    bluetooth_adapter_status_parse(wrong_reply, &status);
    g_assert_false(status.adapter_is_present);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/bluetooth-adapter-status/counted", test_adapter_and_devices_are_counted        },
    {"/bluetooth-adapter-status/missing", test_missing_or_wrong_reply_means_no_adapter},
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
