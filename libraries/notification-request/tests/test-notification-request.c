#include "notification-request.h"

static GVariant *
build_notify_parameters(
    const char *summary,
    GVariant   *hints,
    int32_t     expire_timeout_ms
)
{
    const char *actions[] = {"default", "Open", "reply", "Reply", "odd", NULL};

    return g_variant_ref_sink(
        g_variant_new("(susss^as@a{sv}i)", "Slack", 7, "slack", summary, "<b>body</b>", actions, hints, expire_timeout_ms)
    );
}

/* "transient" has the wrong type on purpose. */
static GVariant *
build_hints(void)
{
    return g_variant_new_parsed(
        "{'urgency': <byte 9>, 'suppress-sound': <true>, 'sound-name': <'message-new-instant'>,"
        " 'transient': <'wrong type'>, 'x-dunst-stack-tag': <'volume'>}"
    );
}

static struct notification_request *
parse_example(
    const char *summary,
    int32_t     expire_timeout_ms,
    uint32_t   *replaces_id
)
{
    g_autoptr(GVariant) parameters = build_notify_parameters(summary, build_hints(), expire_timeout_ms);

    return notification_request_parse(parameters, ":1.42", replaces_id);
}

static void
test_parameters_are_parsed(void)
{
    uint32_t                     replaces_id  = 0;
    struct notification_request *notification = parse_example("New\nmessage\x01", -5, &replaces_id);

    g_assert_cmpuint(replaces_id, ==, 7);
    g_assert_cmpstr(notification->summary, ==, "New message");
    g_assert_cmpuint(g_strv_length(notification->action_keys), ==, 2);
    g_assert_cmpint(notification->expire_timeout_ms, ==, NOTIFICATION_EXPIRES_AS_SERVER_CHOOSES);
    notification_request_unref(notification);
}

/* A hint of the wrong type counts as missing; an urgency above critical is critical. */
static void
test_hints_are_checked(void)
{
    uint32_t                     replaces_id  = 0;
    struct notification_request *notification = parse_example("x", 1000, &replaces_id);

    g_assert_cmpint(notification->urgency, ==, NOTIFICATION_URGENCY_CRITICAL);
    g_assert_true(notification->suppresses_sound && !notification->is_transient);
    g_assert_cmpstr(notification->replacement_tag, ==, "volume");
    g_assert_true(notification_request_has_action(notification, "default"));
    notification_request_unref(notification);
}

static void
test_wrong_parameter_type_is_refused(void)
{
    g_autoptr(GVariant) parameters = g_variant_ref_sink(g_variant_new("(s)", "nonsense"));

    uint32_t replaces_id = 0;

    g_assert_null(notification_request_parse(parameters, ":1.42", &replaces_id));
}

static void
test_long_text_is_capped(void)
{
    g_autofree char *long_text = g_strnfill(5000, 'a');
    g_autofree char *cleaned   = notification_request_clean_text(long_text, 100);

    g_assert_cmpuint(g_utf8_strlen(cleaned, -1), ==, 100);
    g_assert_true(g_str_has_suffix(cleaned, "..."));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/notification-request/parsed",     test_parameters_are_parsed          },
    {"/notification-request/hints",      test_hints_are_checked              },
    {"/notification-request/wrong-type", test_wrong_parameter_type_is_refused},
    {"/notification-request/capped",     test_long_text_is_capped            },
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
