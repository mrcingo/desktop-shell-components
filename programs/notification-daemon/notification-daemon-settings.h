#ifndef NOTIFICATION_DAEMON_SETTINGS_H
#define NOTIFICATION_DAEMON_SETTINGS_H

#include <stdbool.h>

#include "notification-request.h"
#include "notification-rules.h"

/*
 * Settings from notification-daemon.ini. Every one is optional; see
 * notification-daemon.ini in the shipped examples for the defaults.
 */

enum {
    NOTIFICATION_URGENCY_COUNT = NOTIFICATION_URGENCY_CRITICAL + 1,
};

struct notification_daemon_settings {
    enum notification_sound_choice sound_choice;
    int                            sound_volume_percent;
    int                            minimum_sound_interval_ms;
    char                          *sound_player_command;
    char                          *sound_file; // for low and normal urgency
    char                          *critical_sound_file;

    int  timeout_seconds[NOTIFICATION_URGENCY_COUNT]; // 0 for never
    int  history_size;
    bool starts_in_do_not_disturb;
    bool critical_sounds_during_do_not_disturb;

    struct notification_rules *rules;
};

/* override_path is the --config file, or NULL; data_directory holds the shipped sounds. */
struct notification_daemon_settings *notification_daemon_settings_load(const char *override_path, const char *data_directory);

void notification_daemon_settings_destroy(struct notification_daemon_settings *settings);

#endif
