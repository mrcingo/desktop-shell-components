#include <glib.h>

#include "compositor-keyboard-layout.h"

static char *
layout_from_inputs_text(
    const char *json_text
)
{
    g_autoptr(JsonNode) reply = json_from_string(json_text, NULL);

    return compositor_keyboard_layout_from_inputs(reply);
}

static char *
layout_from_event_text(
    const char *json_text
)
{
    g_autoptr(JsonNode) event = json_from_string(json_text, NULL);

    return compositor_keyboard_layout_from_input_event(event);
}

static void
test_first_keyboard_with_layout_counts(void)
{
    g_autofree char *layout = layout_from_inputs_text(
        "[{\"type\": \"pointer\"}, {\"type\": \"keyboard\", \"xkb_active_layout_name\": null},"
        " 7, {\"type\": \"keyboard\", \"xkb_active_layout_name\": \"Spanish\"}]"
    );

    g_assert_cmpstr(layout, ==, "Spanish");
}

static void
test_malformed_replies_have_no_layout(void)
{
    g_autofree char *from_object = layout_from_inputs_text("{\"type\": \"keyboard\"}");
    g_autofree char *from_empty  = layout_from_inputs_text("[]");

    g_assert_null(from_object);
    g_assert_null(from_empty);
    g_assert_null(compositor_keyboard_layout_from_inputs(NULL));
}

static void
test_layout_events_are_read(void)
{
    g_autofree char *changed = layout_from_event_text(
        "{\"change\": \"xkb_layout\", \"input\": {\"type\": \"keyboard\", \"xkb_active_layout_name\": \"English (US)\"}}"
    );
    g_autofree char *other_change = layout_from_event_text(
        "{\"change\": \"libinput_config\", \"input\": {\"type\": \"keyboard\", \"xkb_active_layout_name\": \"English (US)\"}}"
    );

    g_assert_cmpstr(changed, ==, "English (US)");
    g_assert_null(other_change);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/compositor-keyboard-layout/first-keyboard", test_first_keyboard_with_layout_counts},
    {"/compositor-keyboard-layout/malformed",      test_malformed_replies_have_no_layout },
    {"/compositor-keyboard-layout/events",         test_layout_events_are_read           },
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
