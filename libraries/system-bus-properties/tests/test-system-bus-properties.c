#include <glib.h>

#include "system-bus-properties.h"

static GVariant *
build_example_dictionary(void)
{
    g_auto(GVariantBuilder) builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);

    g_variant_builder_add(&builder, "{sv}", "Id", g_variant_new_string("Home"));
    g_variant_builder_add(&builder, "{sv}", "Device", g_variant_new_object_path("/org/example/1"));
    g_variant_builder_add(&builder, "{sv}", "Powered", g_variant_new_boolean(TRUE));
    g_variant_builder_add(&builder, "{sv}", "State", g_variant_new_uint32(4));
    g_variant_builder_add(&builder, "{sv}", "Wrong", g_variant_new_int32(-1));

    return g_variant_ref_sink(g_variant_builder_end(&builder));
}

static void
assert_text_member(
    GVariant   *dictionary,
    const char *key,
    const char *expected_text
)
{
    g_autofree char *text = system_bus_dictionary_lookup_text(dictionary, key);

    g_assert_cmpstr(text, ==, expected_text);
}

static void
test_typed_members_are_read(void)
{
    g_autoptr(GVariant) dictionary = build_example_dictionary();

    uint32_t state = 0;

    assert_text_member(dictionary, "Id", "Home");
    assert_text_member(dictionary, "Device", "/org/example/1");
    g_assert_true(system_bus_dictionary_lookup_boolean(dictionary, "Powered", false));
    g_assert_true(system_bus_dictionary_lookup_unsigned(dictionary, "State", &state));
    g_assert_cmpuint(state, ==, 4);
}

/* Another type, a missing key or a dictionary of the wrong kind all read as absent. */
static void
test_mistyped_members_read_as_absent(void)
{
    g_autoptr(GVariant) dictionary = build_example_dictionary();
    g_autoptr(GVariant) not_a_dict = g_variant_ref_sink(g_variant_new_string("text"));

    uint32_t state = 0;

    g_assert_false(system_bus_dictionary_lookup_unsigned(dictionary, "Wrong", &state));
    assert_text_member(dictionary, "Missing", NULL);
    assert_text_member(not_a_dict, "Id", NULL);
    assert_text_member(NULL, "Id", NULL);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/system-bus-properties/typed",    test_typed_members_are_read         },
    {"/system-bus-properties/mistyped", test_mistyped_members_read_as_absent},
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
