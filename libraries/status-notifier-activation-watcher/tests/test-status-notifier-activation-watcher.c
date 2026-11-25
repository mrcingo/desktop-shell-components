#include <unistd.h>

#include <gio/gio.h>

#include "status-notifier-activation-watcher.h"

enum {
    LONGEST_WAIT_MILLISECONDS = 3000,
};

static const char fake_item_bus_name[] = "org.kde.StatusNotifierItem-test-1";

struct observed_activation {
    GMainLoop *loop;
    int        process_id;
};

static void
remember_activation(
    void *user_data,
    int   process_id
)
{
    struct observed_activation *observed = user_data;

    observed->process_id = process_id;

    g_main_loop_quit(observed->loop);
}

static const struct status_notifier_activation_watcher_handler handler = {
    .handle_item_activated = remember_activation,
};

static GDBusConnection *
connect_to_test_bus(
    GTestDBus *test_bus
)
{
    GDBusConnectionFlags flags = G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION;

    return g_dbus_connection_new_for_address_sync(g_test_dbus_get_bus_address(test_bus), flags, NULL, NULL, NULL);
}

/* The fake icon owns its name but implements nothing; the call itself is what counts. */
static void
click_fake_icon(
    GDBusConnection *bar_connection
)
{
    g_dbus_connection_call(
        bar_connection,
        fake_item_bus_name,
        "/StatusNotifierItem",
        "org.kde.StatusNotifierItem",
        "Activate",
        g_variant_new("(ii)", 10, 10),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        NULL,
        NULL
    );
}

static gboolean
stop_waiting(
    gpointer user_data
)
{
    g_main_loop_quit(user_data);

    return G_SOURCE_CONTINUE;
}

static void
wait_for_activation(
    struct observed_activation *observed
)
{
    guint timeout_source = g_timeout_add(LONGEST_WAIT_MILLISECONDS, stop_waiting, observed->loop);

    g_main_loop_run(observed->loop);
    g_source_remove(timeout_source);
}

/* Synchronous, so the name is owned before the click is sent. */
static void
own_fake_icon_name(
    GDBusConnection *icon_connection
)
{
    g_autoptr(GVariant) reply = g_dbus_connection_call_sync(
        icon_connection,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "RequestName",
        g_variant_new("(su)", fake_item_bus_name, 0),
        G_VARIANT_TYPE("(u)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        NULL
    );

    g_assert_nonnull(reply);
}

static struct status_notifier_activation_watcher *
create_watcher(
    GTestDBus                  *test_bus,
    struct observed_activation *observed
)
{
    g_autoptr(GError) error = NULL;

    struct status_notifier_activation_watcher
        *watcher = status_notifier_activation_watcher_create(g_test_dbus_get_bus_address(test_bus), &handler, observed, &error);

    g_assert_no_error(error);

    return watcher;
}

/* Owns the fake icon's name and clicks it from a second connection; returns the icon's connection, which must stay. */
static GDBusConnection *
click_own_icon(
    GTestDBus *test_bus
)
{
    GDBusConnection *icon_connection = connect_to_test_bus(test_bus);

    g_autoptr(GDBusConnection) bar_connection = connect_to_test_bus(test_bus);

    own_fake_icon_name(icon_connection);
    click_fake_icon(bar_connection);
    g_dbus_connection_flush_sync(bar_connection, NULL, NULL);

    return icon_connection;
}

/* Returns the process the watcher reported for a click on an icon this test process owns. */
static int
report_for_click_on_own_icon(
    GTestDBus *test_bus
)
{
    struct observed_activation                 observed = {.loop = g_main_loop_new(NULL, FALSE)};
    struct status_notifier_activation_watcher *watcher  = create_watcher(test_bus, &observed);

    g_autoptr(GDBusConnection) icon_connection = click_own_icon(test_bus);

    wait_for_activation(&observed);
    status_notifier_activation_watcher_destroy(watcher);
    g_main_loop_unref(observed.loop);

    return observed.process_id;
}

static void
test_icon_click_is_reported(void)
{
    GTestDBus *test_bus = g_test_dbus_new(G_TEST_DBUS_NONE);

    g_test_dbus_up(test_bus);
    g_assert_cmpint(report_for_click_on_own_icon(test_bus), ==, getpid());
    g_test_dbus_down(test_bus);
    g_object_unref(test_bus);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/status-notifier-activation-watcher/click", test_icon_click_is_reported},
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
