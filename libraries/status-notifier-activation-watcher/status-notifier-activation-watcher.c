#include <stdbool.h>

#include "status-notifier-activation-watcher.h"

static const char item_interface_name[]    = "org.kde.StatusNotifierItem";
static const char activation_method_name[] = "Activate";
static const char monitored_calls_rule[]   = "type='method_call',interface='org.kde.StatusNotifierItem',member='Activate'";

/*
 * Shared between the D-Bus worker thread, where filters run, and the main
 * context. The watcher pointer is only read and cleared in the main
 * context, so a call still on its way after destroy finds NULL.
 */
struct shared_state {
    struct status_notifier_activation_watcher *watcher;
    GMainContext                              *main_context;
};

struct status_notifier_activation_watcher {
    GDBusConnection                                         *monitor_connection;
    GDBusConnection                                         *query_connection;
    guint                                                    filter_id;
    struct shared_state                                     *shared;
    const struct status_notifier_activation_watcher_handler *handler;
    void                                                    *user_data;
};

struct reported_call {
    struct shared_state *shared;
    char                *item_bus_name;
};

static void
release_shared_state(
    void *data
)
{
    struct shared_state *shared = data;

    g_main_context_unref(shared->main_context);
}

static void
release_reported_call(
    void *data
)
{
    struct reported_call *call = data;

    g_atomic_rc_box_release_full(call->shared, release_shared_state);
    g_free(call->item_bus_name);
    g_free(call);
}

static int
read_process_id(
    GVariant *reply
)
{
    guint32 process_id = 0;

    g_variant_get(reply, "(u)", &process_id);

    return (int)process_id;
}

/* The watcher may be gone, and the owner may have left the bus meanwhile. */
static void
report_process_id(
    struct status_notifier_activation_watcher *watcher,
    GVariant                                  *reply
)
{
    if (watcher == NULL) {
        return;
    }

    if (reply == NULL) {
        return;
    }

    watcher->handler->handle_item_activated(watcher->user_data, read_process_id(reply));
}

static void
handle_process_id_reply(
    GObject      *connection,
    GAsyncResult *result,
    gpointer      user_data
)
{
    struct reported_call *call = user_data;

    g_autoptr(GVariant) reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(connection), result, NULL);

    report_process_id(call->shared->watcher, reply);
    release_reported_call(call);
}

/* The bus itself knows which process owns the icon's name. */
static gboolean
look_up_item_process(
    gpointer user_data
)
{
    struct reported_call                      *call    = user_data;
    struct status_notifier_activation_watcher *watcher = call->shared->watcher;

    if (watcher == NULL) {
        release_reported_call(call);
        return G_SOURCE_REMOVE;
    }

    g_dbus_connection_call(
        watcher->query_connection,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "GetConnectionUnixProcessID",
        g_variant_new("(s)", call->item_bus_name),
        G_VARIANT_TYPE("(u)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        handle_process_id_reply,
        call
    );

    return G_SOURCE_REMOVE;
}

static bool
is_item_activation(
    GDBusMessage *message
)
{
    if (g_dbus_message_get_message_type(message) != G_DBUS_MESSAGE_TYPE_METHOD_CALL) {
        return false;
    }

    if (g_strcmp0(g_dbus_message_get_interface(message), item_interface_name) != 0) {
        return false;
    }

    return g_strcmp0(g_dbus_message_get_member(message), activation_method_name) == 0;
}

/* Runs in the D-Bus worker thread; hands the call over to the main context. */
static void
report_activation(
    struct shared_state *shared,
    const char          *item_bus_name
)
{
    struct reported_call *call = g_new0(struct reported_call, 1);

    call->shared        = g_atomic_rc_box_acquire(shared);
    call->item_bus_name = g_strdup(item_bus_name);

    g_main_context_invoke(shared->main_context, look_up_item_process, call);
}

/* Replies are the bus answering this connection; everything else is a monitored copy. */
static bool
is_reply(
    GDBusMessage *message
)
{
    GDBusMessageType message_type = g_dbus_message_get_message_type(message);

    if (message_type == G_DBUS_MESSAGE_TYPE_METHOD_RETURN) {
        return true;
    }

    return message_type == G_DBUS_MESSAGE_TYPE_ERROR;
}

static void
report_if_item_activation(
    struct shared_state *shared,
    GDBusMessage        *message
)
{
    const char *item_bus_name = g_dbus_message_get_destination(message);

    if (!is_item_activation(message)) {
        return;
    }

    if (item_bus_name == NULL) {
        return;
    }

    report_activation(shared, item_bus_name);
}

/*
 * Copies must never reach GDBus's own handling, which would answer method
 * calls; a monitor may not send anything. Runs in the D-Bus worker thread.
 */
static GDBusMessage *
filter_monitored_message(
    GDBusConnection *connection,
    GDBusMessage    *message,
    gboolean         is_incoming,
    gpointer         user_data
)
{
    if (!is_incoming) {
        return message; // this connection's own requests, such as BecomeMonitor
    }

    if (is_reply(message)) {
        return message;
    }

    report_if_item_activation(user_data, message);
    g_object_unref(message);

    return NULL;
}

static void
release_filter_reference(
    gpointer data
)
{
    g_atomic_rc_box_release_full(data, release_shared_state);
}

static GDBusConnection *
connect_to_bus(
    const char *bus_address,
    GError    **error
)
{
    GDBusConnectionFlags flags = G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION;
    GDBusConnection     *connection = g_dbus_connection_new_for_address_sync(bus_address, flags, NULL, NULL, error);

    if (connection != NULL) {
        g_dbus_connection_set_exit_on_close(connection, FALSE);
    }

    return connection;
}

static bool
become_monitor(
    GDBusConnection *connection,
    GError         **error
)
{
    const char *const rules[] = {monitored_calls_rule, NULL};

    g_autoptr(GVariant) reply = g_dbus_connection_call_sync(
        connection,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus.Monitoring",
        "BecomeMonitor",
        g_variant_new("(^asu)", rules, 0),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        error
    );

    return reply != NULL;
}

/* The filter holds its own reference on the shared state, released when GDBus drops the filter. */
static void
attach_copy_filter(
    struct status_notifier_activation_watcher *watcher
)
{
    watcher->filter_id = g_dbus_connection_add_filter(
        watcher->monitor_connection,
        filter_monitored_message,
        g_atomic_rc_box_acquire(watcher->shared),
        release_filter_reference
    );
}

static bool
open_monitor_connection(
    struct status_notifier_activation_watcher *watcher,
    const char                                *bus_address,
    GError                                   **error
)
{
    watcher->monitor_connection = connect_to_bus(bus_address, error);

    if (watcher->monitor_connection == NULL) {
        return false;
    }

    attach_copy_filter(watcher);

    return become_monitor(watcher->monitor_connection, error);
}

/* A monitor connection can only receive, so asking for process IDs needs a second one. */
static bool
connect_watcher(
    struct status_notifier_activation_watcher *watcher,
    const char                                *bus_address,
    GError                                   **error
)
{
    if (!open_monitor_connection(watcher, bus_address, error)) {
        return false;
    }

    watcher->query_connection = connect_to_bus(bus_address, error);

    return watcher->query_connection != NULL;
}

static char *
resolve_bus_address(
    const char *bus_address,
    GError    **error
)
{
    if (bus_address != NULL) {
        return g_strdup(bus_address);
    }

    return g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SESSION, NULL, error);
}

static struct status_notifier_activation_watcher *
create_unconnected_watcher(
    const struct status_notifier_activation_watcher_handler *handler,
    void                                                    *user_data
)
{
    struct status_notifier_activation_watcher *watcher = g_new0(struct status_notifier_activation_watcher, 1);

    watcher->handler   = handler;
    watcher->user_data = user_data;
    watcher->shared    = g_atomic_rc_box_new0(struct shared_state);

    watcher->shared->watcher      = watcher;
    watcher->shared->main_context = g_main_context_ref_thread_default();

    return watcher;
}

struct status_notifier_activation_watcher *
status_notifier_activation_watcher_create(
    const char                                              *bus_address,
    const struct status_notifier_activation_watcher_handler *handler,
    void                                                    *user_data,
    GError                                                 **error
)
{
    g_autofree char *resolved_address = resolve_bus_address(bus_address, error);

    if (resolved_address == NULL) {
        return NULL;
    }

    struct status_notifier_activation_watcher *watcher = create_unconnected_watcher(handler, user_data);

    if (!connect_watcher(watcher, resolved_address, error)) {
        g_clear_pointer(&watcher, status_notifier_activation_watcher_destroy);
    }

    return watcher;
}

static void
close_monitor_connection(
    struct status_notifier_activation_watcher *watcher
)
{
    if (watcher->monitor_connection == NULL) {
        return;
    }

    g_dbus_connection_remove_filter(watcher->monitor_connection, watcher->filter_id);
    g_dbus_connection_close_sync(watcher->monitor_connection, NULL, NULL);
    g_object_unref(watcher->monitor_connection);
}

/* Calls still on their way find no watcher and are dropped. */
void
status_notifier_activation_watcher_destroy(
    struct status_notifier_activation_watcher *watcher
)
{
    if (watcher == NULL) {
        return;
    }

    watcher->shared->watcher = NULL;

    close_monitor_connection(watcher);
    g_clear_object(&watcher->query_connection);
    g_atomic_rc_box_release_full(watcher->shared, release_shared_state);
    g_free(watcher);
}
