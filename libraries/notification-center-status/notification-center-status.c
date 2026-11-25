#include <glib.h>

#include "notification-center-status.h"
#include "system-bus-properties.h"

static const char service_name[]   = "org.freedesktop.Notifications";
static const char object_path[]    = "/io/github/mrcingo/DesktopShellComponents/NotificationCenter";
static const char interface_name[] = "io.github.mrcingo.DesktopShellComponents.NotificationCenter";

struct notification_center_status {
    const struct notification_center_status_handler *handler;
    void                                            *user_data;

    guint            name_watch;
    GDBusConnection *connection; // NULL while the name has no owner
    char            *owner_name; // the daemon's unique name
    guint            changes_subscription;
    GCancellable    *reading; // the property read in flight, if any

    struct notification_center_state state;
};

static bool
flags_are_equal(
    const struct notification_center_state *first,
    const struct notification_center_state *second
)
{
    if (first->latest_urgency != second->latest_urgency) {
        return false;
    }

    if (first->is_known != second->is_known) {
        return false;
    }

    return first->does_not_disturb == second->does_not_disturb;
}

static bool
states_are_equal(
    const struct notification_center_state *first,
    const struct notification_center_state *second
)
{
    if (first->is_available != second->is_available) {
        return false;
    }

    if (first->unread_count != second->unread_count) {
        return false;
    }

    if (g_strcmp0(first->latest_text, second->latest_text) != 0) {
        return false;
    }

    return flags_are_equal(first, second);
}

/* Takes ownership of new_state's text. */
static void
report_state(
    struct notification_center_status *status,
    struct notification_center_state  *new_state
)
{
    if (states_are_equal(&status->state, new_state)) {
        g_free(new_state->latest_text);
        return;
    }

    g_free(status->state.latest_text);

    status->state = *new_state;

    status->handler->handle_state_changed(status->user_data, &status->state);
}

static enum notification_urgency
read_urgency(
    GVariant *properties
)
{
    g_autoptr(GVariant) urgency = system_bus_dictionary_lookup(properties, "LatestUrgency", G_VARIANT_TYPE_BYTE);

    if (urgency == NULL) {
        return NOTIFICATION_URGENCY_NORMAL;
    }

    return (enum notification_urgency)MIN(g_variant_get_byte(urgency), NOTIFICATION_URGENCY_CRITICAL);
}

static char *
read_latest_text(
    GVariant *properties
)
{
    char *latest_text = system_bus_dictionary_lookup_text(properties, "LatestText");

    if (latest_text == NULL) {
        return g_strdup("");
    }

    return latest_text;
}

static void
read_available_state(
    struct notification_center_state *state,
    GVariant                         *properties
)
{
    state->is_available     = true;
    state->latest_text      = read_latest_text(properties);
    state->latest_urgency   = read_urgency(properties);
    state->does_not_disturb = system_bus_dictionary_lookup_boolean(properties, "DoNotDisturb", false);
}

/* A daemon without the UnreadCount property is not this one. */
static void
read_properties(
    struct notification_center_status *status,
    GVariant                          *properties
)
{
    struct notification_center_state new_state = {.is_known = true};

    if (system_bus_dictionary_lookup_unsigned(properties, "UnreadCount", &new_state.unread_count)) {
        read_available_state(&new_state, properties);
    }

    report_state(status, &new_state);
}

/* A failed read leaves NULL, which reads as unavailable. */
static GVariant *
first_child(
    GVariant *reply
)
{
    if (reply == NULL) {
        return NULL;
    }

    return g_variant_get_child_value(reply, 0);
}

static void
handle_properties_reply(
    GObject      *connection,
    GAsyncResult *result,
    void         *user_data
)
{
    g_autoptr(GError)   error = NULL;
    g_autoptr(GVariant) reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(connection), result, &error);

    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        return; // the status may be gone
    }

    g_autoptr(GVariant) properties = first_child(reply);

    read_properties(user_data, properties);
}

/* Any earlier read is cancelled: only the newest answer counts. */
static void
request_properties(
    struct notification_center_status *status
)
{
    g_cancellable_cancel(status->reading);
    g_clear_object(&status->reading);

    status->reading = g_cancellable_new();

    g_dbus_connection_call(
        status->connection,
        status->owner_name,
        object_path,
        "org.freedesktop.DBus.Properties",
        "GetAll",
        g_variant_new("(s)", interface_name),
        G_VARIANT_TYPE("(a{sv})"),
        G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1,
        status->reading,
        handle_properties_reply,
        status
    );
}

static void
handle_properties_changed(
    GDBusConnection *connection,
    const char      *sender_name,
    const char      *signal_object_path,
    const char      *signal_interface_name,
    const char      *signal_name,
    GVariant        *parameters,
    void            *user_data
)
{
    request_properties(user_data);
}

/* Only changes of this interface on this object, from the current owner. */
static guint
subscribe_to_changes(
    struct notification_center_status *status
)
{
    return g_dbus_connection_signal_subscribe(
        status->connection,
        status->owner_name,
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        object_path,
        interface_name,
        G_DBUS_SIGNAL_FLAGS_NONE,
        handle_properties_changed,
        status,
        NULL
    );
}

static void
unsubscribe_from_changes(
    struct notification_center_status *status
)
{
    if (status->changes_subscription == 0) {
        return;
    }

    g_dbus_connection_signal_unsubscribe(status->connection, status->changes_subscription);

    status->changes_subscription = 0;
}

static void
forget_owner(
    struct notification_center_status *status
)
{
    g_cancellable_cancel(status->reading);
    g_clear_object(&status->reading);
    unsubscribe_from_changes(status);
    g_clear_object(&status->connection);
    g_clear_pointer(&status->owner_name, g_free);
}

static void
handle_name_appeared(
    GDBusConnection *connection,
    const char      *name,
    const char      *owner_name,
    void            *user_data
)
{
    struct notification_center_status *status = user_data;

    forget_owner(status);

    status->connection           = g_object_ref(connection);
    status->owner_name           = g_strdup(owner_name);
    status->changes_subscription = subscribe_to_changes(status);

    request_properties(status);
}

static void
handle_name_vanished(
    GDBusConnection *connection,
    const char      *name,
    void            *user_data
)
{
    struct notification_center_status *status = user_data;

    struct notification_center_state no_daemon = {.is_known = true};

    forget_owner(status);
    report_state(status, &no_daemon);
}

struct notification_center_status *
notification_center_status_create(
    const struct notification_center_status_handler *handler,
    void                                            *user_data
)
{
    struct notification_center_status *status = g_new0(struct notification_center_status, 1);

    status->handler    = handler;
    status->user_data  = user_data;
    status->name_watch = g_bus_watch_name(
        G_BUS_TYPE_SESSION,
        service_name,
        G_BUS_NAME_WATCHER_FLAGS_NONE,
        handle_name_appeared,
        handle_name_vanished,
        status,
        NULL
    );

    return status;
}

void
notification_center_status_destroy(
    struct notification_center_status *status
)
{
    if (status == NULL) {
        return;
    }

    g_bus_unwatch_name(status->name_watch);
    forget_owner(status);
    g_free(status->state.latest_text);
    g_free(status);
}

const struct notification_center_state *
notification_center_status_get_state(
    const struct notification_center_status *status
)
{
    return &status->state;
}

/* Commands need no answer; a failure shows in the state the daemon reports. */
static void
call_method(
    struct notification_center_status *status,
    const char                        *method_name
)
{
    if (!status->state.is_available) {
        return;
    }

    g_dbus_connection_call(
        status->connection,
        status->owner_name,
        object_path,
        interface_name,
        method_name,
        NULL,
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        NULL,
        NULL
    );
}

void
notification_center_activate_latest(
    struct notification_center_status *status
)
{
    call_method(status, "ActivateLatest");
}

void
notification_center_mark_all_read(
    struct notification_center_status *status
)
{
    call_method(status, "MarkAllRead");
}

void
notification_center_toggle_do_not_disturb(
    struct notification_center_status *status
)
{
    call_method(status, "ToggleDoNotDisturb");
}
