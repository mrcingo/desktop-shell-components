#include <unistd.h>

#include "notification-specification-server.h"

/* What the handler saw. */
struct recorded_calls {
    char *summary;
    int   do_not_disturb_requests;
    bool  last_do_not_disturb;
    bool  name_lost;
    int   sender_process_id;
};

static uint32_t
record_notify(
    void                        *user_data,
    struct notification_request *notification,
    uint32_t                     replaces_id
)
{
    struct recorded_calls *calls = user_data;

    g_free(calls->summary);

    calls->summary = g_strdup(notification->summary);

    notification_request_unref(notification);

    return 42;
}

static void
ignore_id(
    void    *user_data,
    uint32_t id
)
{}

static void
ignore_call(
    void *user_data
)
{}

static void
record_do_not_disturb(
    void *user_data,
    bool  do_not_disturb
)
{
    struct recorded_calls *calls = user_data;

    calls->do_not_disturb_requests++;

    calls->last_do_not_disturb = do_not_disturb;
}

static void
record_name_lost(
    void *user_data
)
{
    ((struct recorded_calls *)user_data)->name_lost = true;
}

static const struct notification_specification_server_handler recording_handler = {
    .handle_notify             = record_notify,
    .handle_close_notification = ignore_id,
    .handle_activate_latest    = ignore_call,
    .handle_mark_all_read      = ignore_call,
    .handle_set_do_not_disturb = record_do_not_disturb,
    .handle_name_lost          = record_name_lost,
};

struct test_bus {
    GTestDBus       *bus;
    GDBusConnection *server_connection;
    GDBusConnection *client_connection;
};

static GDBusConnection *
connect_to(
    GTestDBus *bus
)
{
    GDBusConnectionFlags flags = G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION;

    return g_dbus_connection_new_for_address_sync(g_test_dbus_get_bus_address(bus), flags, NULL, NULL, NULL);
}

static struct test_bus
start_test_bus(void)
{
    struct test_bus test_bus = {.bus = g_test_dbus_new(G_TEST_DBUS_NONE)};

    g_test_dbus_up(test_bus.bus);

    test_bus.server_connection = connect_to(test_bus.bus);
    test_bus.client_connection = connect_to(test_bus.bus);

    return test_bus;
}

static void
stop_test_bus(
    struct test_bus *test_bus
)
{
    g_object_unref(test_bus->server_connection);
    g_object_unref(test_bus->client_connection);
    g_test_dbus_down(test_bus->bus);
    g_object_unref(test_bus->bus);
}

struct pending_reply {
    GVariant *reply;
    GError   *error;
    bool      is_done;
};

static void
store_reply(
    GObject      *connection,
    GAsyncResult *result,
    gpointer      user_data
)
{
    struct pending_reply *pending = user_data;

    pending->reply   = g_dbus_connection_call_finish(G_DBUS_CONNECTION(connection), result, &pending->error);
    pending->is_done = true;
}

/* The server answers from this thread's main loop, so the call cannot block. */
static struct pending_reply
call_and_wait(
    GDBusConnection *connection,
    const char      *object_path,
    const char      *interface_name,
    const char      *method_name,
    GVariant        *parameters
)
{
    struct pending_reply pending = {0};

    g_dbus_connection_call(
        connection,
        "org.freedesktop.Notifications",
        object_path,
        interface_name,
        method_name,
        parameters,
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        5000,
        NULL,
        store_reply,
        &pending
    );

    while (!pending.is_done) {
        g_main_context_iteration(NULL, TRUE);
    }

    return pending;
}

static struct pending_reply
call_notifications(
    struct test_bus *test_bus,
    const char      *method_name,
    GVariant        *parameters
)
{
    return call_and_wait(
        test_bus->client_connection,
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        method_name,
        parameters
    );
}

static GVariant *
build_notify_parameters(void)
{
    const char *no_actions[] = {NULL};

    return g_variant_new("(susss^asa{sv}i)", "Slack", 0, "", "Hello", "", no_actions, NULL, -1);
}

/* A running server on a private bus, with a second connection acting as an application. */
struct server_fixture {
    struct test_bus                           test_bus;
    struct recorded_calls                     calls;
    struct notification_specification_server *server;
};

static void
start_server(
    struct server_fixture *fixture,
    gconstpointer          unused
)
{
    fixture->test_bus = start_test_bus();
    fixture->server   = notification_specification_server_create(
        fixture->test_bus.server_connection,
        "1.0",
        &recording_handler,
        &fixture->calls
    );
}

static void
stop_server(
    struct server_fixture *fixture,
    gconstpointer          unused
)
{
    notification_specification_server_destroy(fixture->server);
    g_free(fixture->calls.summary);
    stop_test_bus(&fixture->test_bus);
}

static void
test_notify_reaches_handler(
    struct server_fixture *fixture,
    gconstpointer          unused
)
{
    struct pending_reply reply = call_notifications(&fixture->test_bus, "Notify", build_notify_parameters());
    uint32_t             id    = 0;

    g_variant_get(reply.reply, "(u)", &id);
    g_assert_cmpuint(id, ==, 42);
    g_assert_cmpstr(fixture->calls.summary, ==, "Hello");
    g_variant_unref(reply.reply);
}

/* Malformed calls never reach the handler. */
static void
test_wrong_arguments_are_refused(
    struct server_fixture *fixture,
    gconstpointer          unused
)
{
    struct pending_reply reply = call_notifications(&fixture->test_bus, "Notify", g_variant_new("(s)", "only one"));

    g_assert_nonnull(reply.error);
    g_assert_null(fixture->calls.summary);
    g_error_free(reply.error);
}

static struct pending_reply
call_center(
    struct test_bus *test_bus,
    const char      *interface_name,
    const char      *method_name,
    GVariant        *parameters
)
{
    return call_and_wait(
        test_bus->client_connection,
        "/io/github/mrcingo/DesktopShellComponents/NotificationCenter",
        interface_name,
        method_name,
        parameters
    );
}

static GVariant *
read_property(
    struct test_bus *test_bus,
    const char      *property_name
)
{
    GVariant *parameters = g_variant_new("(ss)", "io.github.mrcingo.DesktopShellComponents.NotificationCenter", property_name);
    struct pending_reply reply = call_center(test_bus, "org.freedesktop.DBus.Properties", "Get", parameters);
    GVariant            *value = NULL;

    g_variant_get(reply.reply, "(v)", &value);
    g_variant_unref(reply.reply);

    return value;
}

static void
test_center_properties_are_published(
    struct server_fixture *fixture,
    gconstpointer          unused
)
{
    const struct notification_center_state state = {.unread_count = 3, .latest_text = "Slack: Hello"};

    notification_specification_server_publish_center_state(fixture->server, &state);

    g_autoptr(GVariant) unread_count = read_property(&fixture->test_bus, "UnreadCount");
    g_autoptr(GVariant) latest_text  = read_property(&fixture->test_bus, "LatestText");

    g_assert_cmpuint(g_variant_get_uint32(unread_count), ==, 3);
    g_assert_cmpstr(g_variant_get_string(latest_text, NULL), ==, "Slack: Hello");
}

static void
test_do_not_disturb_toggles(
    struct server_fixture *fixture,
    gconstpointer          unused
)
{
    struct pending_reply reply = call_center(
        &fixture->test_bus,
        "io.github.mrcingo.DesktopShellComponents.NotificationCenter",
        "ToggleDoNotDisturb",
        NULL
    );

    g_assert_cmpint(fixture->calls.do_not_disturb_requests, ==, 1);
    g_assert_true(fixture->calls.last_do_not_disturb);
    g_variant_unref(reply.reply);
}

static void
record_sender_process(
    int   process_id,
    void *user_data
)
{
    ((struct recorded_calls *)user_data)->sender_process_id = process_id;
}

static void
wait_for_rival_and_sender(
    const struct recorded_calls *rival_calls,
    const struct recorded_calls *calls
)
{
    while (!rival_calls->name_lost || calls->sender_process_id == 0) {
        g_main_context_iteration(NULL, TRUE);
    }
}

/* A second server leaves the name to the first; the bus tells who sent a call. */
static void
test_second_server_loses_name_and_sender_is_found(
    struct server_fixture *fixture,
    gconstpointer          unused
)
{
    struct recorded_calls                     rival_calls = {0};
    struct notification_specification_server *rival       = notification_specification_server_create(
        fixture->test_bus.client_connection,
        "1.0",
        &recording_handler,
        &rival_calls
    );
    const char *client_name = g_dbus_connection_get_unique_name(fixture->test_bus.client_connection);

    notification_specification_server_find_sender_process(fixture->server, client_name, record_sender_process, &fixture->calls);

    wait_for_rival_and_sender(&rival_calls, &fixture->calls);

    g_assert_cmpint(fixture->calls.sender_process_id, ==, getpid());
    notification_specification_server_destroy(rival);
}

typedef void (*server_test)(struct server_fixture *fixture, gconstpointer unused);

static const struct {
    const char *test_path;
    server_test test_function;
} registered_tests[] = {
    {"/notification-specification-server/notify",       test_notify_reaches_handler                      },
    {"/notification-specification-server/wrong-args",   test_wrong_arguments_are_refused                 },
    {"/notification-specification-server/properties",   test_center_properties_are_published             },
    {"/notification-specification-server/dnd",          test_do_not_disturb_toggles                      },
    {"/notification-specification-server/name-and-pid", test_second_server_loses_name_and_sender_is_found},
};

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);

    for (size_t i = 0; i < G_N_ELEMENTS(registered_tests); i++) {
        g_test_add(
            registered_tests[i].test_path,
            struct server_fixture,
            NULL,
            start_server,
            registered_tests[i].test_function,
            stop_server
        );
    }

    return g_test_run();
}
