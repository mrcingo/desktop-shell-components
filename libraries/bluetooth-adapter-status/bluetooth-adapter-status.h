#ifndef BLUETOOTH_ADAPTER_STATUS_H
#define BLUETOOTH_ADAPTER_STATUS_H

#include <stdbool.h>

#include <gio/gio.h>

/* The Bluetooth adapter and its connected devices, as BlueZ reports them on D-Bus. */

struct bluetooth_adapter_status {
    bool  adapter_is_present;
    bool  is_powered;
    int   connected_device_count;
    char *adapter_path; // NULL without an adapter
};

/* Replaces status with a fresh reading; without BlueZ or an adapter, adapter_is_present is false. */
void bluetooth_adapter_status_read(GDBusConnection *connection, struct bluetooth_adapter_status *status);

/* Reads BlueZ's GetManagedObjects reply, of type (a{oa{sa{sv}}}). */
void bluetooth_adapter_status_parse(GVariant *managed_objects_reply, struct bluetooth_adapter_status *status);

void bluetooth_adapter_status_clear(struct bluetooth_adapter_status *status);

/* Turns the adapter on or off; BlueZ may refuse, which is logged. */
void bluetooth_adapter_set_powered(GDBusConnection *connection, const struct bluetooth_adapter_status *status, bool is_powered);

#endif
