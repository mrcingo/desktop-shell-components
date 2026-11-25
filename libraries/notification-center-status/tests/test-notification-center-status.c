#include <gio/gio.h>

#include "notification-center-status.h"

/* A stand-in for the notification daemon, on a private bus the test starts itself. */

static const char
    fake_interface_xml[] = "<node><interface name='io.github.mrcingo.DesktopShellComponents.NotificationCenter'>"
                           "<property name='UnreadCount' type='u' access='read'/>"
                           "<property name='LatestText' type='s' access='read'/>"
                           "<property name='LatestUrgency' type='y' access='read'/>"
                           "<property name='DoNotDisturb' type='b' access='readwrite'/>"
                           "<method name='ActivateLatest'/><method name='MarkAllRead'/><method name='ToggleDoNotDisturb'/>"
                           "</interface></node>";

static const char object_path[]    = "/io/github/mrcingo/DesktopShellComponents/NotificationCenter";
static const char interface_name[] = "io.github.mrcingo.DesktopShellComponents.NotificationCenter";

struct fake_daemon {
    GDBusConnection *connection;
    guint            registration;
    guint32          unread_count;
    const char      *latest_text;
    guchar           latest_urgency;
    gboolean         does_not_disturb;
    int              activation_count;
};

static GVariant *
get_fake_text_property(
    struct fake_daemon *daemon,
    const char         *property_name
)
{
    if (g_strcmp0(property_name, "LatestText") == 0) {
        return g_variant_new_string(daemon->latest_text);
    }

    return g_variant_new_byte(daemon->latest_urgency);
}

static GVariant *
get_fake_property(
    GDBusConnection *connection,
    const char      *sender,
    const char      *path,
    const char      *interface,
    const char      *property_name,
    GError         **error,
    void            *user_data
)
{
    struct fake_daemon *daemon = user_data;

    if (g_strcmp0(property_name, "UnreadCount") == 0) {
        return g_variant_new_uint32(daemon->unread_count);
    }

    if (g_strcmp0(property_name, "DoNotDisturb") == 0) {
        return g_variant_new_boolean(daemon->does_not_disturb);
    }

    return get_fake_text_property(daemon, property_name);
}

/* The real daemon announces changes the same way; the client reads everything again. */
static void
announce_change(
    struct fake_daemon *daemon
)
{
    g_dbus_connection_emit_signal(
        daemon->connection,
        NULL,
        object_path,
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        g_variant_new_parsed("(%s, @a{sv} {}, @as [])", interface_name),
        NULL
    );
}

static void
mark_fake_notifications_read(
    struct fake_daemon *daemon
)
{
    daemon->unread_count = 0;
    daemon->latest_text  = "";
}

static void
perform_fake_method(
    struct fake_daemon *daemon,
    const char         *method_name
)
{
    if (g_strcmp0(method_name, "ActivateLatest") == 0) {
        daemon->activation_count++;
    }

    if (g_strcmp0(method_name, "ToggleDoNotDisturb") == 0) {
        daemon->does_not_disturb = !daemon->does_not_disturb;
    }

    if (g_strcmp0(method_name, "MarkAllRead") == 0) {
        mark_fake_notifications_read(daemon);
    }
}

static void
handle_fake_method(
    GDBusConnection       *connection,
    const char            *sender,
    const char            *path,
    const char            *interface,
    const char            *method_name,
    GVariant              *parameters,
    GDBusMethodInvocation *invocation,
    void                  *user_data
)
{
    perform_fake_method(user_data, method_name);
    announce_change(user_data);
    g_dbus_method_invocation_return_value(invocation, NULL);
}

static const GDBusInterfaceVTable fake_vtable = {
    .method_call  = handle_fake_method,
    .get_property = get_fake_property,
};

static GDBusConnection *
connect_to_test_bus(
    GTestDBus *bus
)
{
    return g_dbus_connection_new_for_address_sync(
        g_test_dbus_get_bus_address(bus),
        G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
        NULL,
        NULL,
        NULL
    );
}

static void
start_fake_daemon(
    struct fake_daemon *daemon,
    GTestDBus          *bus
)
{
    g_autoptr(GDBusNodeInfo) node = g_dbus_node_info_new_for_xml(fake_interface_xml, NULL);

    daemon->connection   = connect_to_test_bus(bus);
    daemon->registration = g_dbus_connection_register_object(
        daemon->connection,
        object_path,
        node->interfaces[0],
        &fake_vtable,
        daemon,
        NULL,
        NULL
    );

    g_bus_own_name_on_connection(
        daemon->connection,
        "org.freedesktop.Notifications",
        G_BUS_NAME_OWNER_FLAGS_NONE,
        NULL,
        NULL,
        NULL,
        NULL
    );
}

static void
stop_fake_daemon(
    struct fake_daemon *daemon
)
{
    g_dbus_connection_unregister_object(daemon->connection, daemon->registration);
    g_dbus_connection_close_sync(daemon->connection, NULL, NULL);
    g_clear_object(&daemon->connection);
}

static void
record_state(
    void                                   *user_data,
    const struct notification_center_state *state
)
{}

static const struct notification_center_status_handler handler = {
    .handle_state_changed = record_state,
};

/* Lets both ends exchange messages for a while. */
static void
exchange_messages(void)
{
    for (int i = 0; i < 200; i++) {
        g_main_context_iteration(NULL, FALSE);
        g_usleep(1000);
    }
}

static void
assert_state(
    const struct notification_center_status *status,
    guint32                                  unread_count,
    const char                              *latest_text,
    bool                                     does_not_disturb
)
{
    const struct notification_center_state *state = notification_center_status_get_state(status);

    g_assert_true(state->is_available);
    g_assert_cmpuint(state->unread_count, ==, unread_count);
    g_assert_cmpstr(state->latest_text, ==, latest_text);
    g_assert_cmpint(state->does_not_disturb, ==, does_not_disturb);
}

static void
assert_daemon_gone(
    const struct notification_center_status *status
)
{
    const struct notification_center_state *state = notification_center_status_get_state(status);

    g_assert_false(state->is_available);
    g_assert_null(state->latest_text);
}

static struct notification_center_status *
start_watching(
    struct fake_daemon *daemon,
    GTestDBus          *bus
)
{
    g_test_dbus_up(bus);
    start_fake_daemon(daemon, bus);

    struct notification_center_status *status = notification_center_status_create(&handler, NULL);

    exchange_messages();

    return status;
}

/* The daemon leaves first, so the status sees the name lose its owner. */
static void
stop_watching(
    struct fake_daemon                *daemon,
    struct notification_center_status *status,
    GTestDBus                         *bus
)
{
    stop_fake_daemon(daemon);
    exchange_messages();
    assert_daemon_gone(status);
    notification_center_status_destroy(status);
    g_test_dbus_down(bus);
}

static void
use_commands(
    struct notification_center_status *status
)
{
    notification_center_toggle_do_not_disturb(status);
    exchange_messages();
    assert_state(status, 3, "Mail: Hello", true);
    notification_center_mark_all_read(status);
    notification_center_activate_latest(status);
    exchange_messages();
}

static void
assert_first_state(
    const struct notification_center_status *status
)
{
    g_assert_cmpint(notification_center_status_get_state(status)->latest_urgency, ==, NOTIFICATION_URGENCY_CRITICAL);
    assert_state(status, 3, "Mail: Hello", false);
}

/* Marking all read cleared the text; activating reached the daemon. */
static void
assert_commands_arrived(
    const struct fake_daemon                *daemon,
    const struct notification_center_status *status
)
{
    g_assert_cmpint(daemon->activation_count, ==, 1);
    assert_state(status, 0, "", true);
}

/* Unread count, latest text, urgency and do-not-disturb, the commands, then the daemon going away. */
static void
test_daemon_is_followed(void)
{
    g_autoptr(GTestDBus) bus = g_test_dbus_new(G_TEST_DBUS_NONE);

    struct fake_daemon                 daemon = {.unread_count = 3, .latest_text = "Mail: Hello", .latest_urgency = 2};
    struct notification_center_status *status = start_watching(&daemon, bus);

    assert_first_state(status);
    use_commands(status);
    assert_commands_arrived(&daemon, status);
    stop_watching(&daemon, status, bus);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/notification-center-status/daemon", test_daemon_is_followed},
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
