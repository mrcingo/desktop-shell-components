#ifndef SYSTEM_SERVICE_READINGS_H
#define SYSTEM_SERVICE_READINGS_H

#include "battery-charge-status.h"
#include "bluetooth-adapter-status.h"
#include "network-manager-status.h"
#include "power-profile-status.h"

/*
 * What the machine's system services report on D-Bus: network
 * (NetworkManager), Bluetooth (BlueZ), power profile
 * (power-profiles-daemon) and battery (UPower). Values are read again only
 * when a service signals a change that concerns them, and several signals
 * in a row cause one reading. A missing service simply reads as absent and
 * is picked up when it starts.
 */

enum system_service_reading {
    SYSTEM_SERVICE_READING_NETWORK       = 1 << 0,
    SYSTEM_SERVICE_READING_BLUETOOTH     = 1 << 1,
    SYSTEM_SERVICE_READING_POWER_PROFILE = 1 << 2,
    SYSTEM_SERVICE_READING_BATTERY       = 1 << 3,
};

struct system_service_values {
    struct network_manager_status   network;
    struct bluetooth_adapter_status bluetooth;
    struct power_profile_status     power_profile;
    struct battery_charge_status    battery;
};

struct system_service_readings_handler {
    /* After values were read again because a service changed something. */
    void (*handle_values_changed)(void *user_data, const struct system_service_values *values);
};

/* Reads the services asked for (enum system_service_reading flags) at once. */
struct system_service_readings *
system_service_readings_create(unsigned readings, const struct system_service_readings_handler *handler, void *user_data);

void system_service_readings_destroy(struct system_service_readings *readings);

const struct system_service_values *system_service_readings_get_values(const struct system_service_readings *readings);

void system_service_readings_toggle_bluetooth(struct system_service_readings *readings);

/* Switches to the next power profile (power-saver, balanced, performance). */
void system_service_readings_switch_power_profile(struct system_service_readings *readings);

#endif
