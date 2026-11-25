#include <string.h>

#include <glib.h>

#include "network-manager-status.h"
#include "system-bus-properties.h"

static const char service_name[]                = "org.freedesktop.NetworkManager";
static const char manager_path[]                = "/org/freedesktop/NetworkManager";
static const char active_connection_interface[] = "org.freedesktop.NetworkManager.Connection.Active";
static const char device_interface[]            = "org.freedesktop.NetworkManager.Device";
static const char wireless_interface[]          = "org.freedesktop.NetworkManager.Device.Wireless";
static const char access_point_interface[]      = "org.freedesktop.NetworkManager.AccessPoint";

/* Active connections looked at for a VPN; more would only cost time. */
enum {
    MAXIMUM_CHECKED_CONNECTIONS = 16,
    WIRELESS_DEVICE_TYPE        = 2,
};

static const struct {
    const char                  *connection_type;
    enum network_connection_kind kind;
} connection_kinds[] = {
    {"802-3-ethernet",  NETWORK_CONNECTION_KIND_WIRED   },
    {"802-11-wireless", NETWORK_CONNECTION_KIND_WIRELESS},
};

enum network_connection_kind
network_connection_kind_from_type(
    const char *connection_type
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(connection_kinds); i++) {
        if (g_strcmp0(connection_type, connection_kinds[i].connection_type) == 0) {
            return connection_kinds[i].kind;
        }
    }

    return NETWORK_CONNECTION_KIND_OTHER;
}

enum network_connectivity
network_connectivity_from_state(
    uint32_t connectivity_state
)
{
    if (connectivity_state > NETWORK_CONNECTIVITY_FULL) {
        return NETWORK_CONNECTIVITY_UNKNOWN;
    }

    return (enum network_connectivity)connectivity_state;
}

/* NetworkManager uses "/" for "no object". */
static char *
take_object_path(
    char *object_path
)
{
    if (g_strcmp0(object_path, "/") == 0) {
        g_clear_pointer(&object_path, g_free);
    }

    return object_path;
}

static bool
connection_is_vpn(
    GDBusConnection *connection,
    const char      *connection_path
)
{
    g_autoptr(GVariant)
        properties = system_bus_properties_read_all(connection, service_name, connection_path, active_connection_interface);
    g_autofree char *connection_type = system_bus_dictionary_lookup_text(properties, "Type");

    if (system_bus_dictionary_lookup_boolean(properties, "Vpn", false)) {
        return true;
    }

    return g_strcmp0(connection_type, "wireguard") == 0;
}

static gsize
count_children(
    GVariant *container
)
{
    if (container == NULL) {
        return 0;
    }

    return g_variant_n_children(container);
}

static bool
any_connection_is_vpn(
    GDBusConnection *connection,
    GVariant        *manager_properties
)
{
    g_autoptr(GVariant) paths = system_bus_dictionary_lookup(
        manager_properties,
        "ActiveConnections",
        G_VARIANT_TYPE_OBJECT_PATH_ARRAY
    );

    gsize path_count = count_children(paths);

    for (gsize i = 0; i < MIN(path_count, MAXIMUM_CHECKED_CONNECTIONS); i++) {
        g_autoptr(GVariant) path = g_variant_get_child_value(paths, i);

        if (connection_is_vpn(connection, g_variant_get_string(path, NULL))) {
            return true;
        }
    }

    return false;
}

static void
read_manager(
    GDBusConnection               *connection,
    GVariant                      *manager_properties,
    struct network_manager_status *status
)
{
    uint32_t connectivity_state = 0;

    system_bus_dictionary_lookup_unsigned(manager_properties, "Connectivity", &connectivity_state);

    status->service_is_running      = true;
    status->connectivity            = network_connectivity_from_state(connectivity_state);
    status->primary_connection_path = take_object_path(
        system_bus_dictionary_lookup_text(manager_properties, "PrimaryConnection")
    );
    status->vpn_is_active = any_connection_is_vpn(connection, manager_properties);
}

/* The first device of the connection, which carries its traffic. */
static char *
first_device_path(
    GVariant *connection_properties
)
{
    g_autoptr(GVariant) devices = system_bus_dictionary_lookup(
        connection_properties,
        "Devices",
        G_VARIANT_TYPE_OBJECT_PATH_ARRAY
    );

    if (devices == NULL) {
        return NULL;
    }

    if (g_variant_n_children(devices) == 0) {
        return NULL;
    }

    g_autoptr(GVariant) first_device = g_variant_get_child_value(devices, 0);

    return g_variant_dup_string(first_device, NULL);
}

static void
read_primary_connection(
    GDBusConnection               *connection,
    struct network_manager_status *status
)
{
    g_autoptr(GVariant) properties = system_bus_properties_read_all(
        connection,
        service_name,
        status->primary_connection_path,
        active_connection_interface
    );
    g_autofree char *connection_type = system_bus_dictionary_lookup_text(properties, "Type");

    if (properties == NULL) {
        return;
    }

    status->connection_kind = network_connection_kind_from_type(connection_type);
    status->connection_name = system_bus_dictionary_lookup_text(properties, "Id");
    status->device_path     = take_object_path(first_device_path(properties));
}

static bool
is_empty_text(
    const char *text
)
{
    if (text == NULL) {
        return true;
    }

    return text[0] == '\0';
}

/* The IP interface carries the traffic (ppp0 over a modem); the device interface is the fallback. */
static char *
read_interface_name(
    GVariant *device_properties
)
{
    g_autofree char *ip_interface_name = system_bus_dictionary_lookup_text(device_properties, "IpInterface");

    if (is_empty_text(ip_interface_name)) {
        return system_bus_dictionary_lookup_text(device_properties, "Interface");
    }

    return g_steal_pointer(&ip_interface_name);
}

static int
read_access_point_strength(
    GDBusConnection *connection,
    const char      *access_point_path
)
{
    g_autoptr(GVariant)
        properties = system_bus_properties_read_all(connection, service_name, access_point_path, access_point_interface);
    g_autoptr(GVariant) strength = system_bus_dictionary_lookup(properties, "Strength", G_VARIANT_TYPE_BYTE);

    if (strength == NULL) {
        return -1;
    }

    return MIN(100, g_variant_get_byte(strength));
}

static void
read_wireless_device(
    GDBusConnection               *connection,
    struct network_manager_status *status
)
{
    g_autoptr(GVariant)
        properties = system_bus_properties_read_all(connection, service_name, status->device_path, wireless_interface);

    status->access_point_path = take_object_path(system_bus_dictionary_lookup_text(properties, "ActiveAccessPoint"));

    if (status->access_point_path == NULL) {
        return;
    }

    status->wifi_signal_percent = read_access_point_strength(connection, status->access_point_path);
}

static void
read_device(
    GDBusConnection               *connection,
    struct network_manager_status *status
)
{
    g_autoptr(GVariant)
        properties = system_bus_properties_read_all(connection, service_name, status->device_path, device_interface);

    uint32_t device_type = 0;

    status->interface_name = read_interface_name(properties);

    if (!system_bus_dictionary_lookup_unsigned(properties, "DeviceType", &device_type)) {
        return;
    }

    if (device_type == WIRELESS_DEVICE_TYPE) {
        read_wireless_device(connection, status);
    }
}

/* Each step only runs when the one before found its object. */
static void
read_connection_chain(
    GDBusConnection               *connection,
    struct network_manager_status *status
)
{
    if (status->primary_connection_path == NULL) {
        return;
    }

    read_primary_connection(connection, status);

    if (status->device_path == NULL) {
        return;
    }

    read_device(connection, status);
}

void
network_manager_status_read(
    GDBusConnection               *connection,
    struct network_manager_status *status
)
{
    g_autoptr(GVariant)
        manager_properties = system_bus_properties_read_all(connection, service_name, manager_path, service_name);

    network_manager_status_clear(status);

    if (manager_properties == NULL) {
        return;
    }

    read_manager(connection, manager_properties, status);
    read_connection_chain(connection, status);
}

void
network_manager_status_clear(
    struct network_manager_status *status
)
{
    g_free(status->connection_name);
    g_free(status->interface_name);
    g_free(status->primary_connection_path);
    g_free(status->device_path);
    g_free(status->access_point_path);

    *status = (struct network_manager_status){.wifi_signal_percent = -1};
}

bool
network_manager_status_concerns_object(
    const struct network_manager_status *status,
    const char                          *object_path
)
{
    const char *const relevant_paths[] =
        {manager_path, status->primary_connection_path, status->device_path, status->access_point_path};

    if (object_path == NULL) {
        return true;
    }

    for (size_t i = 0; i < G_N_ELEMENTS(relevant_paths); i++) {
        if (g_strcmp0(object_path, relevant_paths[i]) == 0) {
            return true;
        }
    }

    return false;
}
