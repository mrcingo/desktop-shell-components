#ifndef BATTERY_CHARGE_STATUS_H
#define BATTERY_CHARGE_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#include <gio/gio.h>

/*
 * The battery as UPower sums it up for display (its "DisplayDevice", which
 * combines every battery of the machine): charge, whether it is charging
 * and the time left.
 */

enum battery_charge_state {
    BATTERY_CHARGE_STATE_UNKNOWN,
    BATTERY_CHARGE_STATE_CHARGING,
    BATTERY_CHARGE_STATE_DISCHARGING,
    BATTERY_CHARGE_STATE_EMPTY,
    BATTERY_CHARGE_STATE_FULLY_CHARGED,
    BATTERY_CHARGE_STATE_PENDING_CHARGE,
    BATTERY_CHARGE_STATE_PENDING_DISCHARGE,
};

struct battery_charge_status {
    bool                      is_present; // false without a battery or without UPower
    int                       percent;    // 0 to 100
    enum battery_charge_state state;
    int64_t                   seconds_until_empty; // 0 when unknown
    int64_t                   seconds_until_full;  // 0 when unknown
};

/* The UPower object path whose signals change the status. */
extern const char battery_charge_status_object_path[];

void battery_charge_status_read(GDBusConnection *connection, struct battery_charge_status *status);

/* Reads org.freedesktop.UPower.Device properties, an a{sv}. */
void battery_charge_status_parse(GVariant *device_properties, struct battery_charge_status *status);

#endif
