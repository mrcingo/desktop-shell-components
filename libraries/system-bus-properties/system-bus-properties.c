#include <glib.h>

#include "system-bus-properties.h"

enum {
    CALL_TIMEOUT_MILLISECONDS = 1000,
};

static const char properties_interface[] = "org.freedesktop.DBus.Properties";

GVariant *
system_bus_call_without_arguments(
    GDBusConnection    *connection,
    const char         *service_name,
    const char         *object_path,
    const char         *interface_name,
    const char         *method_name,
    const GVariantType *reply_type
)
{
    return g_dbus_connection_call_sync(
        connection,
        service_name,
        object_path,
        interface_name,
        method_name,
        NULL,
        reply_type,
        G_DBUS_CALL_FLAGS_NO_AUTO_START,
        CALL_TIMEOUT_MILLISECONDS,
        NULL,
        NULL
    );
}

static GVariant *
call_get_all(
    GDBusConnection *connection,
    const char      *service_name,
    const char      *object_path,
    const char      *interface_name
)
{
    return g_dbus_connection_call_sync(
        connection,
        service_name,
        object_path,
        properties_interface,
        "GetAll",
        g_variant_new("(s)", interface_name),
        G_VARIANT_TYPE("(a{sv})"),
        G_DBUS_CALL_FLAGS_NO_AUTO_START,
        CALL_TIMEOUT_MILLISECONDS,
        NULL,
        NULL
    );
}

GVariant *
system_bus_properties_read_all(
    GDBusConnection *connection,
    const char      *service_name,
    const char      *object_path,
    const char      *interface_name
)
{
    g_autoptr(GVariant) reply = NULL;

    if (!g_variant_is_object_path(object_path)) {
        return NULL;
    }

    reply = call_get_all(connection, service_name, object_path, interface_name);

    if (reply == NULL) {
        return NULL;
    }

    return g_variant_get_child_value(reply, 0);
}

static void
report_write_result(
    GObject      *connection,
    GAsyncResult *result,
    void         *user_data
)
{
    g_autoptr(GError)   error = NULL;
    g_autoptr(GVariant) reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(connection), result, &error);

    if (reply == NULL) {
        g_message("Cannot change a setting: %s", error->message);
    }
}

void
system_bus_property_write(
    GDBusConnection *connection,
    const char      *service_name,
    const char      *object_path,
    const char      *interface_name,
    const char      *property_name,
    GVariant        *value
)
{
    g_dbus_connection_call(
        connection,
        service_name,
        object_path,
        properties_interface,
        "Set",
        g_variant_new("(ssv)", interface_name, property_name, value),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        CALL_TIMEOUT_MILLISECONDS,
        NULL,
        report_write_result,
        NULL
    );
}

GVariant *
system_bus_dictionary_lookup(
    GVariant           *dictionary,
    const char         *key,
    const GVariantType *expected_type
)
{
    if (dictionary == NULL) {
        return NULL;
    }

    if (!g_variant_is_of_type(dictionary, G_VARIANT_TYPE_VARDICT)) {
        return NULL;
    }

    return g_variant_lookup_value(dictionary, key, expected_type);
}

char *
system_bus_dictionary_lookup_text(
    GVariant   *dictionary,
    const char *key
)
{
    g_autoptr(GVariant) text = system_bus_dictionary_lookup(dictionary, key, G_VARIANT_TYPE_STRING);

    if (text == NULL) {
        text = system_bus_dictionary_lookup(dictionary, key, G_VARIANT_TYPE_OBJECT_PATH);
    }

    if (text == NULL) {
        return NULL;
    }

    return g_variant_dup_string(text, NULL);
}

bool
system_bus_dictionary_lookup_boolean(
    GVariant   *dictionary,
    const char *key,
    bool        fallback_value
)
{
    g_autoptr(GVariant) value = system_bus_dictionary_lookup(dictionary, key, G_VARIANT_TYPE_BOOLEAN);

    if (value == NULL) {
        return fallback_value;
    }

    return g_variant_get_boolean(value);
}

bool
system_bus_dictionary_lookup_unsigned(
    GVariant   *dictionary,
    const char *key,
    uint32_t   *value
)
{
    g_autoptr(GVariant) found_value = system_bus_dictionary_lookup(dictionary, key, G_VARIANT_TYPE_UINT32);

    if (found_value == NULL) {
        return false;
    }

    *value = g_variant_get_uint32(found_value);

    return true;
}

struct system_bus_service_watch {
    GDBusConnection *connection;
    guint            service_signals_subscription;
    guint            owner_changes_subscription;
    void (*handle_signal)(void *user_data, const char *object_path);
    void *user_data;
};

static void
handle_service_signal(
    GDBusConnection *connection,
    const char      *sender_name,
    const char      *object_path,
    const char      *interface_name,
    const char      *signal_name,
    GVariant        *parameters,
    void            *user_data
)
{
    struct system_bus_service_watch *watch = user_data;

    watch->handle_signal(watch->user_data, object_path);
}

static void
handle_owner_change(
    GDBusConnection *connection,
    const char      *sender_name,
    const char      *object_path,
    const char      *interface_name,
    const char      *signal_name,
    GVariant        *parameters,
    void            *user_data
)
{
    struct system_bus_service_watch *watch = user_data;

    watch->handle_signal(watch->user_data, NULL);
}

static guint
subscribe_to_service_signals(
    struct system_bus_service_watch *watch,
    const char                      *service_name
)
{
    return g_dbus_connection_signal_subscribe(
        watch->connection,
        service_name,
        NULL,
        NULL,
        NULL,
        NULL,
        G_DBUS_SIGNAL_FLAGS_NONE,
        handle_service_signal,
        watch,
        NULL
    );
}

/* NameOwnerChanged with the service's name as first argument: it started, stopped or restarted. */
static guint
subscribe_to_owner_changes(
    struct system_bus_service_watch *watch,
    const char                      *service_name
)
{
    return g_dbus_connection_signal_subscribe(
        watch->connection,
        "org.freedesktop.DBus",
        "org.freedesktop.DBus",
        "NameOwnerChanged",
        "/org/freedesktop/DBus",
        service_name,
        G_DBUS_SIGNAL_FLAGS_NONE,
        handle_owner_change,
        watch,
        NULL
    );
}

struct system_bus_service_watch *
system_bus_service_watch_create(
    GDBusConnection *connection,
    const char      *service_name,
    void (*handle_signal)(void *user_data, const char *object_path),
    void *user_data
)
{
    struct system_bus_service_watch *watch = g_new0(struct system_bus_service_watch, 1);

    *watch = (struct system_bus_service_watch){
        .connection    = g_object_ref(connection),
        .handle_signal = handle_signal,
        .user_data     = user_data,
    };

    watch->service_signals_subscription = subscribe_to_service_signals(watch, service_name);
    watch->owner_changes_subscription   = subscribe_to_owner_changes(watch, service_name);

    return watch;
}

void
system_bus_service_watch_destroy(
    struct system_bus_service_watch *watch
)
{
    if (watch == NULL) {
        return;
    }

    g_dbus_connection_signal_unsubscribe(watch->connection, watch->service_signals_subscription);
    g_dbus_connection_signal_unsubscribe(watch->connection, watch->owner_changes_subscription);
    g_object_unref(watch->connection);
    g_free(watch);
}
