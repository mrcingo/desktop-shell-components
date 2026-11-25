#ifndef NETWORK_MANAGER_STATUS_H
#define NETWORK_MANAGER_STATUS_H

#include <stdbool.h>

#include <gio/gio.h>

/*
 * What NetworkManager knows about the machine's network, read from its
 * D-Bus interface: whether the internet is reachable, the connection in use
 * and, for Wi-Fi, its signal strength.
 */

enum network_connectivity {
    NETWORK_CONNECTIVITY_UNKNOWN,
    NETWORK_CONNECTIVITY_NONE,    // no network at all
    NETWORK_CONNECTIVITY_PORTAL,  // behind a login page
    NETWORK_CONNECTIVITY_LIMITED, // a network, but no internet
    NETWORK_CONNECTIVITY_FULL,
};

enum network_connection_kind {
    NETWORK_CONNECTION_KIND_NONE,
    NETWORK_CONNECTION_KIND_WIRED,
    NETWORK_CONNECTION_KIND_WIRELESS,
    NETWORK_CONNECTION_KIND_OTHER, // mobile broadband, VPN as primary, bridges, ...
};

struct network_manager_status {
    bool                         service_is_running;
    enum network_connectivity    connectivity;
    enum network_connection_kind connection_kind;
    char                        *connection_name;     // NULL without a connection
    char                        *interface_name;      // NULL without a device, such as wlan0
    int                          wifi_signal_percent; // -1 unless on Wi-Fi
    bool                         vpn_is_active;

    /* The objects whose changes alter this status; NULL when absent. */
    char *primary_connection_path;
    char *device_path;
    char *access_point_path;
};

/* Replaces status with a fresh reading; a stopped service leaves service_is_running false. */
void network_manager_status_read(GDBusConnection *connection, struct network_manager_status *status);

void network_manager_status_clear(struct network_manager_status *status);

/* Whether a signal from object_path can change the status; NULL (the service restarted) always can. */
bool network_manager_status_concerns_object(const struct network_manager_status *status, const char *object_path);

/* NetworkManager's connection type ("802-11-wireless", "802-3-ethernet", ...). */
enum network_connection_kind network_connection_kind_from_type(const char *connection_type);

/* NetworkManager's NMConnectivityState number. */
enum network_connectivity network_connectivity_from_state(uint32_t connectivity_state);

#endif
