#ifndef POWER_PROFILE_STATUS_H
#define POWER_PROFILE_STATUS_H

#include <gio/gio.h>

/*
 * The power profile (power-saver, balanced, performance) chosen through
 * power-profiles-daemon on D-Bus, and switching to the next one.
 */

struct power_profile_status {
    char  *active_profile;     // NULL when the service is not running
    char **available_profiles; // NULL-terminated, in the service's order; NULL when unknown
};

void power_profile_status_read(GDBusConnection *connection, struct power_profile_status *status);

void power_profile_status_clear(struct power_profile_status *status);

/*
 * The profile after current in the order power-saver, balanced, performance,
 * wrapping around and skipping profiles the machine lacks. NULL when there
 * is nothing to switch to.
 */
const char *power_profile_following(const char *current_profile, const char *const *available_profiles);

/* Asks the service to switch; it may refuse, which is logged. */
void power_profile_activate(GDBusConnection *connection, const char *profile);

#endif
