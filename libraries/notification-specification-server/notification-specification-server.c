#include "notification-specification-server.h"

static const char notifications_bus_name[]    = "org.freedesktop.Notifications";
static const char notifications_object_path[] = "/org/freedesktop/Notifications";
static const char notifications_interface[]   = "org.freedesktop.Notifications";
static const char center_object_path[]        = "/io/github/mrcingo/DesktopShellComponents/NotificationCenter";
static const char center_interface[]          = "io.github.mrcingo.DesktopShellComponents.NotificationCenter";

/* Notifications are kept for the status bar, which shows that they came and can bring up their window. */
static const char *const capabilities[] = {"actions", "sound", NULL};

static const char introspection_xml[] = "<node>"
                                        "  <interface name='org.freedesktop.Notifications'>"
                                        "    <method name='GetCapabilities'><arg type='as' direction='out'/></method>"
                                        "    <method name='Notify'>"
                                        "      <arg type='s' direction='in'/><arg type='u' direction='in'/>"
                                        "      <arg type='s' direction='in'/><arg type='s' direction='in'/>"
                                        "      <arg type='s' direction='in'/><arg type='as' direction='in'/>"
                                        "      <arg type='a{sv}' direction='in'/><arg type='i' direction='in'/>"
                                        "      <arg type='u' direction='out'/>"
                                        "    </method>"
                                        "    <method name='CloseNotification'><arg type='u' direction='in'/></method>"
                                        "    <method name='GetServerInformation'>"
                                        "      <arg type='s' direction='out'/><arg type='s' direction='out'/>"
                                        "      <arg type='s' direction='out'/><arg type='s' direction='out'/>"
                                        "    </method>"
                                        "    <signal name='NotificationClosed'><arg type='u'/><arg type='u'/></signal>"
                                        "    <signal name='ActionInvoked'><arg type='u'/><arg type='s'/></signal>"
                                        "    <signal name='ActivationToken'><arg type='u'/><arg type='s'/></signal>"
                                        "  </interface>"
                                        "  <interface name='io.github.mrcingo.DesktopShellComponents.NotificationCenter'>"
                                        "    <property name='UnreadCount' type='u' access='read'/>"
                                        "    <property name='LatestText' type='s' access='read'/>"
                                        "    <property name='LatestUrgency' type='y' access='read'/>"
                                        "    <property name='DoNotDisturb' type='b' access='readwrite'/>"
                                        "    <method name='ActivateLatest'/>"
                                        "    <method name='MarkAllRead'/>"
                                        "    <method name='ToggleDoNotDisturb'/>"
                                        "  </interface>"
                                        "</node>";

struct notification_specification_server {
    GDBusConnection *connection;
    GDBusNodeInfo   *node_info;
    guint            notifications_registration;
    guint            center_registration;
    guint            name_ownership;
    char            *server_version;

    const struct notification_specification_server_handler *handler;
    void                                                   *user_data;

    guint                     unread_count;
    char                     *latest_text;
    enum notification_urgency latest_urgency;
    bool                      do_not_disturb;
};

typedef void (*method_implementation)(
    struct notification_specification_server *server,
    GDBusMethodInvocation                    *invocation,
    GVariant                                 *parameters
);

static void
reply_with_notify(
    struct notification_specification_server *server,
    GDBusMethodInvocation                    *invocation,
    GVariant                                 *parameters
)
{
    uint32_t                     replaces_id  = 0;
    struct notification_request *notification = notification_request_parse(
        parameters,
        g_dbus_method_invocation_get_sender(invocation),
        &replaces_id
    );

    if (notification == NULL) {
        g_dbus_method_invocation_return_dbus_error(
            invocation,
            "org.freedesktop.DBus.Error.InvalidArgs",
            "Notify takes (susssasa{sv}i)"
        );

        return;
    }

    uint32_t id = server->handler->handle_notify(server->user_data, notification, replaces_id);

    g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", id));
}

static void
reply_with_close_notification(
    struct notification_specification_server *server,
    GDBusMethodInvocation                    *invocation,
    GVariant                                 *parameters
)
{
    uint32_t id = 0;

    g_variant_get(parameters, "(u)", &id);
    server->handler->handle_close_notification(server->user_data, id);
    g_dbus_method_invocation_return_value(invocation, NULL);
}

static void
reply_with_capabilities(
    struct notification_specification_server *server,
    GDBusMethodInvocation                    *invocation,
    GVariant                                 *parameters
)
{
    g_dbus_method_invocation_return_value(invocation, g_variant_new("(^as)", capabilities));
}

static void
reply_with_server_information(
    struct notification_specification_server *server,
    GDBusMethodInvocation                    *invocation,
    GVariant                                 *parameters
)
{
    g_dbus_method_invocation_return_value(
        invocation,
        g_variant_new("(ssss)", "desktop-shell-notification-daemon", "desktop-shell-components", server->server_version, "1.2")
    );
}

static void
reply_with_activate_latest(
    struct notification_specification_server *server,
    GDBusMethodInvocation                    *invocation,
    GVariant                                 *parameters
)
{
    server->handler->handle_activate_latest(server->user_data);
    g_dbus_method_invocation_return_value(invocation, NULL);
}

static void
reply_with_mark_all_read(
    struct notification_specification_server *server,
    GDBusMethodInvocation                    *invocation,
    GVariant                                 *parameters
)
{
    server->handler->handle_mark_all_read(server->user_data);
    g_dbus_method_invocation_return_value(invocation, NULL);
}

static void
reply_with_toggle_do_not_disturb(
    struct notification_specification_server *server,
    GDBusMethodInvocation                    *invocation,
    GVariant                                 *parameters
)
{
    server->handler->handle_set_do_not_disturb(server->user_data, !server->do_not_disturb);
    g_dbus_method_invocation_return_value(invocation, NULL);
}

/* GDBus has already checked each call's argument types against the introspection data. */
static const struct {
    const char           *method_name;
    method_implementation implementation;
} methods[] = {
    {"Notify",               reply_with_notify               },
    {"CloseNotification",    reply_with_close_notification   },
    {"GetCapabilities",      reply_with_capabilities         },
    {"GetServerInformation", reply_with_server_information   },
    {"ActivateLatest",       reply_with_activate_latest      },
    {"MarkAllRead",          reply_with_mark_all_read        },
    {"ToggleDoNotDisturb",   reply_with_toggle_do_not_disturb},
};

static void
handle_method_call(
    GDBusConnection       *connection,
    const char            *sender,
    const char            *object_path,
    const char            *interface_name,
    const char            *method_name,
    GVariant              *parameters,
    GDBusMethodInvocation *invocation,
    gpointer               user_data
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(methods); i++) {
        if (g_strcmp0(method_name, methods[i].method_name) == 0) {
            methods[i].implementation(user_data, invocation, parameters);
            return;
        }
    }

    g_dbus_method_invocation_return_dbus_error(invocation, "org.freedesktop.DBus.Error.UnknownMethod", "No such method");
}

static GVariant *
read_center_property(
    struct notification_specification_server *server,
    const char                               *property_name
)
{
    if (g_strcmp0(property_name, "UnreadCount") == 0) {
        return g_variant_new_uint32(server->unread_count);
    }

    if (g_strcmp0(property_name, "LatestText") == 0) {
        return g_variant_new_string(server->latest_text);
    }

    if (g_strcmp0(property_name, "LatestUrgency") == 0) {
        return g_variant_new_byte((guint8)server->latest_urgency);
    }

    return g_variant_new_boolean(server->do_not_disturb);
}

static GVariant *
handle_get_property(
    GDBusConnection *connection,
    const char      *sender,
    const char      *object_path,
    const char      *interface_name,
    const char      *property_name,
    GError         **error,
    gpointer         user_data
)
{
    return read_center_property(user_data, property_name);
}

/* Only DoNotDisturb is writable, which GDBus checks with the introspection data. */
static gboolean
handle_set_property(
    GDBusConnection *connection,
    const char      *sender,
    const char      *object_path,
    const char      *interface_name,
    const char      *property_name,
    GVariant        *value,
    GError         **error,
    gpointer         user_data
)
{
    struct notification_specification_server *server = user_data;

    server->handler->handle_set_do_not_disturb(server->user_data, g_variant_get_boolean(value));

    return TRUE;
}

static const GDBusInterfaceVTable interface_vtable = {
    .method_call  = handle_method_call,
    .get_property = handle_get_property,
    .set_property = handle_set_property,
};

static void
handle_name_lost(
    GDBusConnection *connection,
    const char      *name,
    gpointer         user_data
)
{
    struct notification_specification_server *server = user_data;

    server->handler->handle_name_lost(server->user_data);
}

static guint
register_interface(
    struct notification_specification_server *server,
    const char                               *object_path,
    const char                               *interface_name
)
{
    GDBusInterfaceInfo *interface_info = g_dbus_node_info_lookup_interface(server->node_info, interface_name);

    return g_dbus_connection_register_object(
        server->connection,
        object_path,
        interface_info,
        &interface_vtable,
        server,
        NULL,
        NULL
    );
}

/* Another server keeps the name rather than queueing for it; this one then reports the loss. */
static void
register_and_own_name(
    struct notification_specification_server *server
)
{
    server->notifications_registration = register_interface(server, notifications_object_path, notifications_interface);
    server->center_registration        = register_interface(server, center_object_path, center_interface);
    server->name_ownership             = g_bus_own_name_on_connection(
        server->connection,
        notifications_bus_name,
        G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE,
        NULL,
        handle_name_lost,
        server,
        NULL
    );
}

struct notification_specification_server *
notification_specification_server_create(
    GDBusConnection                                        *connection,
    const char                                             *server_version,
    const struct notification_specification_server_handler *handler,
    void                                                   *user_data
)
{
    struct notification_specification_server *server = g_new0(struct notification_specification_server, 1);

    *server = (struct notification_specification_server){
        .connection     = g_object_ref(connection),
        .node_info      = g_dbus_node_info_new_for_xml(introspection_xml, NULL),
        .server_version = g_strdup(server_version),
        .handler        = handler,
        .user_data      = user_data,
        .latest_text    = g_strdup(""),
        .latest_urgency = NOTIFICATION_URGENCY_NORMAL,
    };

    register_and_own_name(server);

    return server;
}

/* Also lets go of the connection. */
static void
leave_bus(
    struct notification_specification_server *server
)
{
    g_bus_unown_name(server->name_ownership);
    g_dbus_connection_unregister_object(server->connection, server->notifications_registration);
    g_dbus_connection_unregister_object(server->connection, server->center_registration);
    g_dbus_node_info_unref(server->node_info);
    g_object_unref(server->connection);
}

void
notification_specification_server_destroy(
    struct notification_specification_server *server
)
{
    if (server == NULL) {
        return;
    }

    leave_bus(server);
    g_free(server->server_version);
    g_free(server->latest_text);
    g_free(server);
}

static void
emit_notifications_signal(
    struct notification_specification_server *server,
    const char                               *signal_name,
    GVariant                                 *parameters
)
{
    g_dbus_connection_emit_signal(
        server->connection,
        NULL,
        notifications_object_path,
        notifications_interface,
        signal_name,
        parameters,
        NULL
    );
}

void
notification_specification_server_emit_closed(
    struct notification_specification_server *server,
    uint32_t                                  id,
    enum notification_closed_reason           reason
)
{
    emit_notifications_signal(server, "NotificationClosed", g_variant_new("(uu)", id, (uint32_t)reason));
}

void
notification_specification_server_emit_action_invoked(
    struct notification_specification_server *server,
    uint32_t                                  id,
    const char                               *action_key
)
{
    emit_notifications_signal(server, "ActionInvoked", g_variant_new("(us)", id, action_key));
}

/* Each publish_... adds the property to changed when its value differs, and remembers the new value. */
static void
publish_unread_count(
    struct notification_specification_server *server,
    guint                                     unread_count,
    GVariantDict                             *changed
)
{
    if (server->unread_count == unread_count) {
        return;
    }

    server->unread_count = unread_count;

    g_variant_dict_insert(changed, "UnreadCount", "u", unread_count);
}

static void
publish_latest_text(
    struct notification_specification_server *server,
    const char                               *latest_text,
    GVariantDict                             *changed
)
{
    if (g_strcmp0(server->latest_text, latest_text) == 0) {
        return;
    }

    g_free(server->latest_text);

    server->latest_text = g_strdup(latest_text);

    g_variant_dict_insert(changed, "LatestText", "s", latest_text);
}

static void
publish_latest_urgency(
    struct notification_specification_server *server,
    enum notification_urgency                 latest_urgency,
    GVariantDict                             *changed
)
{
    if (server->latest_urgency == latest_urgency) {
        return;
    }

    server->latest_urgency = latest_urgency;

    g_variant_dict_insert(changed, "LatestUrgency", "y", (guint8)latest_urgency);
}

static void
publish_do_not_disturb(
    struct notification_specification_server *server,
    bool                                      do_not_disturb,
    GVariantDict                             *changed
)
{
    if (server->do_not_disturb == do_not_disturb) {
        return;
    }

    server->do_not_disturb = do_not_disturb;

    g_variant_dict_insert(changed, "DoNotDisturb", "b", do_not_disturb);
}

static void
emit_properties_changed(
    struct notification_specification_server *server,
    GVariant                                 *changed_properties
)
{
    const char *const no_invalidated_properties[] = {NULL};
    GVariant *parameters = g_variant_new("(s@a{sv}^as)", center_interface, changed_properties, no_invalidated_properties);

    g_dbus_connection_emit_signal(
        server->connection,
        NULL,
        center_object_path,
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        parameters,
        NULL
    );
}

/* changed is consumed; nothing is announced when it is empty. */
static void
publish_if_changed(
    struct notification_specification_server *server,
    GVariant                                 *changed
)
{
    g_variant_ref_sink(changed);

    if (g_variant_n_children(changed) > 0) {
        emit_properties_changed(server, changed);
    }

    g_variant_unref(changed);
}

void
notification_specification_server_publish_center_state(
    struct notification_specification_server *server,
    const struct notification_center_state   *state
)
{
    GVariantDict changed;

    g_variant_dict_init(&changed, NULL);
    publish_unread_count(server, state->unread_count, &changed);
    publish_latest_text(server, state->latest_text, &changed);
    publish_latest_urgency(server, state->latest_urgency, &changed);
    publish_do_not_disturb(server, state->do_not_disturb, &changed);
    publish_if_changed(server, g_variant_dict_end(&changed));
}

struct sender_process_request {
    notification_sender_process_callback callback;
    void                                *user_data;
};

static void
handle_sender_process_reply(
    GObject      *connection,
    GAsyncResult *result,
    gpointer      user_data
)
{
    struct sender_process_request *request = user_data;

    g_autoptr(GVariant) reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(connection), result, NULL);

    uint32_t process_id = 0;

    if (reply != NULL) {
        g_variant_get(reply, "(u)", &process_id);
    }

    request->callback((int)MIN(process_id, (uint32_t)G_MAXINT), request->user_data);
    g_free(request);
}

void
notification_specification_server_find_sender_process(
    struct notification_specification_server *server,
    const char                               *sender_bus_name,
    notification_sender_process_callback      callback,
    void                                     *user_data
)
{
    struct sender_process_request *request = g_new0(struct sender_process_request, 1);

    *request = (struct sender_process_request){.callback = callback, .user_data = user_data};

    g_dbus_connection_call(
        server->connection,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "GetConnectionUnixProcessID",
        g_variant_new("(s)", sender_bus_name),
        G_VARIANT_TYPE("(u)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        handle_sender_process_reply,
        request
    );
}
