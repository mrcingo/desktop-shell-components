#include <glib.h>

#include "notification-daemon-settings.h"
#include "program-configuration-file.h"

static const struct program_configuration_key known_keys[] = {
    {"sound",          "play-sound"      },
    {"sound",          "volume"          },
    {"sound",          "minimum-interval"},
    {"sound",          "player"          },
    {"sound",          "file"            },
    {"sound",          "critical-file"   },
    {"timeouts",       "low"             },
    {"timeouts",       "normal"          },
    {"timeouts",       "critical"        },
    {"history",        "size"            },
    {"do-not-disturb", "on-at-start"     },
    {"do-not-disturb", "critical-sounds" },
    {"app:*",          "play-sound"      },
    {"app:*",          "urgency"         },
    {"app:*",          "timeout"         },
    {"app:*",          "ignore"          },
    {NULL,             NULL              },
};

/* Seconds after which a notification counts as expired, by urgency; critical ones never expire. */
static const int default_timeout_seconds[NOTIFICATION_URGENCY_COUNT] = {5, 10, 0};

enum {
    DEFAULT_VOLUME_PERCENT   = 70,
    DEFAULT_MINIMUM_INTERVAL = 1500,
    LONGEST_MINIMUM_INTERVAL = 60 * 1000,
    DEFAULT_HISTORY_SIZE     = 100,
    LARGEST_HISTORY_SIZE     = 1000,
    LONGEST_TIMEOUT_SECONDS  = 24 * 60 * 60,
};

static char *
read_sound_file(
    struct program_configuration_file *configuration,
    const char                        *key_name,
    const char                        *data_directory,
    const char                        *shipped_file_name
)
{
    g_autofree char *shipped_path = g_build_filename(data_directory, shipped_file_name, NULL);

    return program_configuration_file_read_text(configuration, "sound", key_name, shipped_path);
}

static void
read_sound_settings(
    struct notification_daemon_settings *settings,
    struct program_configuration_file   *configuration,
    const char                          *data_directory
)
{
    settings->sound_choice = program_configuration_file_read_choice(
        configuration,
        "sound",
        "play-sound",
        notification_sound_choice_names,
        NOTIFICATION_SOUND_UNLESS_SUPPRESSED
    );
    settings->sound_volume_percent = program_configuration_file_read_integer(
        configuration,
        "sound",
        "volume",
        DEFAULT_VOLUME_PERCENT,
        0,
        100
    );
    settings->minimum_sound_interval_ms = program_configuration_file_read_integer(
        configuration,
        "sound",
        "minimum-interval",
        DEFAULT_MINIMUM_INTERVAL,
        0,
        LONGEST_MINIMUM_INTERVAL
    );
    settings->sound_player_command = program_configuration_file_read_text(
        configuration,
        "sound",
        "player",
        "pw-play --volume={volume}"
    );
    settings->sound_file          = read_sound_file(configuration, "file", data_directory, "notification-chime.wav");
    settings->critical_sound_file = read_sound_file(
        configuration,
        "critical-file",
        data_directory,
        "notification-critical-chime.wav"
    );
}

static void
read_timeouts(
    struct notification_daemon_settings *settings,
    struct program_configuration_file   *configuration
)
{
    for (int urgency = 0; urgency < NOTIFICATION_URGENCY_COUNT; urgency++) {
        settings->timeout_seconds[urgency] = program_configuration_file_read_integer(
            configuration,
            "timeouts",
            notification_urgency_names[urgency],
            default_timeout_seconds[urgency],
            0,
            LONGEST_TIMEOUT_SECONDS
        );
    }
}

static void
read_other_settings(
    struct notification_daemon_settings *settings,
    struct program_configuration_file   *configuration
)
{
    settings->history_size = program_configuration_file_read_integer(
        configuration,
        "history",
        "size",
        DEFAULT_HISTORY_SIZE,
        1,
        LARGEST_HISTORY_SIZE
    );
    settings->starts_in_do_not_disturb = program_configuration_file_read_boolean(
        configuration,
        "do-not-disturb",
        "on-at-start",
        false
    );
    settings->critical_sounds_during_do_not_disturb = program_configuration_file_read_boolean(
        configuration,
        "do-not-disturb",
        "critical-sounds",
        true
    );
    settings->rules = notification_rules_load(configuration);
}

static void
read_all_settings(
    struct notification_daemon_settings *settings,
    struct program_configuration_file   *configuration,
    const char                          *data_directory
)
{
    program_configuration_file_report_unknown_keys(configuration, known_keys);
    read_sound_settings(settings, configuration, data_directory);
    read_timeouts(settings, configuration);
    read_other_settings(settings, configuration);
}

struct notification_daemon_settings *
notification_daemon_settings_load(
    const char *override_path,
    const char *data_directory
)
{
    struct program_configuration_file   *configuration = program_configuration_file_load("notification-daemon", override_path);
    struct notification_daemon_settings *settings      = g_new0(struct notification_daemon_settings, 1);

    read_all_settings(settings, configuration, data_directory);
    program_configuration_file_destroy(configuration);

    return settings;
}

void
notification_daemon_settings_destroy(
    struct notification_daemon_settings *settings
)
{
    if (settings == NULL) {
        return;
    }

    notification_rules_destroy(settings->rules);
    g_free(settings->sound_player_command);
    g_free(settings->sound_file);
    g_free(settings->critical_sound_file);
    g_free(settings);
}
