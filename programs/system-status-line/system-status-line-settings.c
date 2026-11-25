#include <stddef.h>
#include <string.h>

#include <glib.h>

#include "program-configuration-file.h"
#include "system-status-line-settings.h"

/*
 * The defaults follow what widely shared status bar setups settle on: the
 * blocks a laptop user checks most, short labels, warning colors before a
 * problem and urgent ones when it is one, scrolling to change levels.
 */

struct format_default {
    const char *key;
    const char *text;
};

struct block_description {
    const char                  *name;
    const struct format_default *formats; // ends with {NULL, NULL}
    const char                  *mouse_actions[SYSTEM_STATUS_LINE_MOUSE_SLOT_COUNT];
    int                          warning_threshold;
    int                          urgent_threshold;
    unsigned                     interval_seconds; // 0 for blocks that are told about changes
};

/* An empty format hides the block in that state. */
static const struct format_default notification_formats[] = {
    {"format-unread",         "{text}{more}{dnd}"},
    {"format-read",           "0"                },
    {"format-do-not-disturb", "DND"              },
    {"format-unavailable",    "NOTIF off"        },
    {NULL,                    NULL               },
};

static const struct format_default attention_formats[] = {
    {"format",      "ATTN {count}"},
    {"format-none", ""            },
    {NULL,          NULL          },
};

static const struct format_default network_formats[] = {
    {"format-wifi",         "WIFI {name} {signal}%{vpn}"},
    {"format-ethernet",     "ETH {name}{vpn}"           },
    {"format-other",        "NET {name}{vpn}"           },
    {"format-disconnected", "NET offline"               },
    {"format-no-internet",  "NET {name} no internet"    },
    {"format-portal",       "NET {name} login needed"   },
    {NULL,                  NULL                        },
};

static const struct format_default traffic_formats[] = {
    {"format", "RX {down} TX {up}"},
    {NULL,     NULL               },
};

static const struct format_default bluetooth_formats[] = {
    {"format-off",       "BT off"        },
    {"format-on",        "BT on"         },
    {"format-connected", "BT {connected}"},
    {NULL,               NULL            },
};

static const struct format_default processor_formats[] = {
    {"format", "CPU {usage}%"},
    {NULL,     NULL          },
};

static const struct format_default temperature_formats[] = {
    {"format", "TEMP {celsius}C"},
    {NULL,     NULL             },
};

static const struct format_default memory_formats[] = {
    {"format", "MEM {used}/{total}"},
    {NULL,     NULL                },
};

static const struct format_default disk_formats[] = {
    {"format", "DISK {free}"},
    {NULL,     NULL         },
};

static const struct format_default volume_formats[] = {
    {"format",       "VOL {volume}%"},
    {"format-muted", "VOL muted"    },
    {NULL,           NULL           },
};

static const struct format_default brightness_formats[] = {
    {"format", "BRI {percent}%"},
    {NULL,     NULL            },
};

static const struct format_default power_profile_formats[] = {
    {"format", "PWR {profile}"},
    {NULL,     NULL           },
};

static const struct format_default battery_formats[] = {
    {"format-discharging", "BAT {percent}% {time}"    },
    {"format-charging",    "BAT {percent}% CHR {time}"},
    {"format-full",        "BAT full"                 },
    {"format-other",       "BAT {percent}%"           },
    {NULL,                 NULL                       },
};

static const struct format_default keyboard_layout_formats[] = {
    {"format", "{layout}"},
    {NULL,     NULL      },
};

static const struct format_default clock_formats[] = {
    {"format",     "%a %d %b %H:%M"   },
    {"format-alt", "%Y-%m-%d %H:%M:%S"},
    {NULL,         NULL               },
};

/* In the order of enum system_status_line_block_kind. Mouse actions: left, middle, right, scroll up, scroll down. */
static const struct block_description block_descriptions[SYSTEM_STATUS_LINE_BLOCK_KIND_COUNT] = {
    {"notifications",
     notification_formats,                       {"activate-notification", "mark-notifications-read", "toggle-do-not-disturb", "", ""},
     -1,
     -1,
     0                                                                                                                                            },
    {"attention",       attention_formats,       {"focus-urgent-window", "", "", "", ""},                                               -1, -1, 0 },
    {"network",         network_formats,         {"nm-connection-editor", "", "", "", ""},                                              -1, -1, 0 },
    {"traffic",         traffic_formats,         {"", "", "", "", ""},                                                                  -1, -1, 2 },
    {"bluetooth",       bluetooth_formats,       {"toggle-bluetooth", "", "", "", ""},                                                  -1, -1, 0 },
    {"processor",       processor_formats,       {"", "", "", "", ""},                                                                  80, 95, 2 },
    {"temperature",     temperature_formats,     {"", "", "", "", ""},                                                                  80, 90, 5 },
    {"memory",          memory_formats,          {"", "", "", "", ""},                                                                  80, 95, 5 },
    {"disk",            disk_formats,            {"", "", "", "", ""},                                                                  15, 5,  30},
    {"volume",          volume_formats,          {"toggle-mute", "", "pavucontrol", "volume-up", "volume-down"},                        -1, -1, 0 },
    {"brightness",      brightness_formats,      {"", "", "", "brightness-up", "brightness-down"},                                      -1, -1, 2 },
    {"power-profile",   power_profile_formats,   {"next-power-profile", "", "", "", ""},                                                -1, -1, 0 },
    {"battery",         battery_formats,         {"", "", "", "", ""},                                                                  20, 10, 0 },
    {"keyboard-layout", keyboard_layout_formats, {"", "", "", "", ""},                                                                  -1, -1, 0 },
    {"clock",           clock_formats,           {"toggle-format", "", "", "", ""},                                                     -1, -1, 0 },
};

static const char *const mouse_action_keys[SYSTEM_STATUS_LINE_MOUSE_SLOT_COUNT] =
    {"left-click", "middle-click", "right-click", "scroll-up", "scroll-down"};

static const char
    default_block_order[] = "attention network traffic bluetooth processor temperature memory disk volume brightness "
                            "power-profile battery keyboard-layout clock notifications";

enum {
    LARGEST_THRESHOLD        = 1000,
    LARGEST_INTERVAL_SECONDS = 3600,
};

/* Keys every block group accepts besides its formats. */
static const char *const common_block_keys[] =
    {"warning", "urgent", "interval", "left-click", "middle-click", "right-click", "scroll-up", "scroll-down", NULL};

/* Keys that only one group accepts. */
static const struct program_configuration_key special_keys[] = {
    {"status",        "order"                            },
    {"status",        "separator-width"                  },
    {"status",        "text-color"                       },
    {"status",        "warning-color"                    },
    {"status",        "pad-numbers"                      },
    {"status",        "accent-color"                     },
    {"status",        "accent-text-color"                },
    {"notifications", "width"                            },
    {"notifications", "new-notification-emphasis-seconds"},
    {"notifications", "show-when-empty"                  },
    {"status",        "quiet-color"                      },
    {"status",        "unread-color"                     },
    {"status",        "critical-color"                   },
    {"disk",          "path"                             },
    {"temperature",   "sensor"                           },
    {"volume",        "step"                             },
    {"brightness",    "step"                             },
};

const char *
system_status_line_block_name(
    enum system_status_line_block_kind kind
)
{
    return block_descriptions[kind].name;
}

static bool
find_kind_by_name(
    const char                         *name,
    enum system_status_line_block_kind *kind
)
{
    for (size_t i = 0; i < SYSTEM_STATUS_LINE_BLOCK_KIND_COUNT; i++) {
        if (strcmp(name, block_descriptions[i].name) == 0) {
            *kind = (enum system_status_line_block_kind)i;
            return true;
        }
    }

    return false;
}

/* Unknown names are reported; a block listed twice is shown once. */
static void
add_shown_block(
    struct system_status_line_settings *settings,
    const char                         *name
)
{
    enum system_status_line_block_kind kind = 0;

    if (!find_kind_by_name(name, &kind)) {
        g_warning("[status] order: there is no block named \"%s\"", name);
        return;
    }

    if (system_status_line_settings_shows(settings, kind)) {
        return;
    }

    settings->shown_blocks[settings->shown_block_count++] = kind;
}

/* A list such as "network, battery clock": spaces and commas both separate. */
static void
read_block_order(
    struct system_status_line_settings *settings,
    struct program_configuration_file  *configuration
)
{
    g_autofree char *order_text = program_configuration_file_read_text(configuration, "status", "order", default_block_order);
    g_auto(GStrv)    names      = g_strsplit_set(order_text, " ,\t", -1);

    for (char **name = names; *name != NULL; name++) {
        if (**name != '\0') {
            add_shown_block(settings, *name);
        }
    }
}

static GHashTable *
read_formats(
    const struct block_description    *description,
    struct program_configuration_file *configuration
)
{
    GHashTable *formats = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    for (const struct format_default *format = description->formats; format->key != NULL; format++) {
        char *text = program_configuration_file_read_text(configuration, description->name, format->key, format->text);

        g_hash_table_insert(formats, g_strdup(format->key), text);
    }

    return formats;
}

static void
read_mouse_actions(
    struct system_status_line_block_settings *block,
    const struct block_description           *description,
    struct program_configuration_file        *configuration
)
{
    for (size_t i = 0; i < SYSTEM_STATUS_LINE_MOUSE_SLOT_COUNT; i++) {
        block->mouse_actions[i] = program_configuration_file_read_text(
            configuration,
            description->name,
            mouse_action_keys[i],
            description->mouse_actions[i]
        );
    }
}

static int
read_threshold(
    struct program_configuration_file *configuration,
    const char                        *group_name,
    const char                        *key_name,
    int                                default_value
)
{
    return program_configuration_file_read_integer(configuration, group_name, key_name, default_value, -1, LARGEST_THRESHOLD);
}

/* Blocks that are told about changes have no interval. */
static unsigned
read_interval(
    const struct block_description    *description,
    struct program_configuration_file *configuration
)
{
    if (description->interval_seconds == 0) {
        return 0;
    }

    return (unsigned)program_configuration_file_read_integer(
        configuration,
        description->name,
        "interval",
        (int)description->interval_seconds,
        1,
        LARGEST_INTERVAL_SECONDS
    );
}

static void
read_block(
    struct system_status_line_block_settings *block,
    const struct block_description           *description,
    struct program_configuration_file        *configuration
)
{
    block->formats           = read_formats(description, configuration);
    block->warning_threshold = read_threshold(configuration, description->name, "warning", description->warning_threshold);
    block->urgent_threshold  = read_threshold(configuration, description->name, "urgent", description->urgent_threshold);
    block->interval_seconds  = read_interval(description, configuration);

    read_mouse_actions(block, description, configuration);
}

static bool
has_color_length(
    size_t length
)
{
    if (length == 7) {
        return true;
    }

    return length == 9;
}

/* "#rrggbb" or "#rrggbbaa", as swaybar accepts. */
static bool
is_color(
    const char *text
)
{
    size_t length = strlen(text);

    if (text[0] != '#') {
        return false;
    }

    if (!has_color_length(length)) {
        return false;
    }

    return strspn(text + 1, "0123456789abcdefABCDEF") == length - 1;
}

static char *
read_color(
    struct program_configuration_file *configuration,
    const char                        *key_name,
    const char                        *default_color
)
{
    char *color = program_configuration_file_read_text(configuration, "status", key_name, default_color);

    if (is_color(color)) {
        return color;
    }

    g_warning("[status] %s is not a color such as #bbbbbb; using %s", key_name, default_color);
    g_free(color);

    return g_strdup(default_color);
}

static void
read_notification_colors(
    struct system_status_line_settings *settings,
    struct program_configuration_file  *configuration
)
{
    settings->quiet_color    = read_color(configuration, "quiet-color", "#808080");
    settings->unread_color   = read_color(configuration, "unread-color", "#5fafd7");
    settings->critical_color = read_color(configuration, "critical-color", "#ff5f5f");
}

static void
read_notification_settings(
    struct system_status_line_settings *settings,
    struct program_configuration_file  *configuration
)
{
    settings->notification_width = program_configuration_file_read_integer(configuration, "notifications", "width", 50, 4, 500);
    settings->new_notification_emphasis_seconds = program_configuration_file_read_integer(
        configuration,
        "notifications",
        "new-notification-emphasis-seconds",
        4,
        0,
        60
    );
    settings->shows_empty_notifications = program_configuration_file_read_boolean(
        configuration,
        "notifications",
        "show-when-empty",
        true
    );
}

static void
read_appearance(
    struct system_status_line_settings *settings,
    struct program_configuration_file  *configuration
)
{
    settings->text_color        = read_color(configuration, "text-color", "#bbbbbb");
    settings->warning_color     = read_color(configuration, "warning-color", "#e0b040");
    settings->accent_color      = read_color(configuration, "accent-color", "#005577");
    settings->accent_text_color = read_color(configuration, "accent-text-color", "#eeeeee");

    read_notification_colors(settings, configuration);

    settings->separator_width = program_configuration_file_read_integer(configuration, "status", "separator-width", 15, 0, 100);
    settings->pads_numbers    = program_configuration_file_read_boolean(configuration, "status", "pad-numbers", true);
}

static void
read_block_specific_settings(
    struct system_status_line_settings *settings,
    struct program_configuration_file  *configuration
)
{
    read_notification_settings(settings, configuration);

    settings->disk_path               = program_configuration_file_read_text(configuration, "disk", "path", "/");
    settings->temperature_sensor_name = program_configuration_file_read_text(configuration, "temperature", "sensor", "k10temp");
    settings->volume_step_percent     = program_configuration_file_read_integer(configuration, "volume", "step", 5, 1, 50);
    settings->brightness_step_percent = program_configuration_file_read_integer(configuration, "brightness", "step", 5, 1, 50);
}

static void
add_block_keys(
    GArray                         *keys,
    const struct block_description *description
)
{
    for (const struct format_default *format = description->formats; format->key != NULL; format++) {
        g_array_append_val(keys, ((struct program_configuration_key){description->name, format->key}));
    }

    for (const char *const *key = common_block_keys; *key != NULL; key++) {
        g_array_append_val(keys, ((struct program_configuration_key){description->name, *key}));
    }
}

/* Every accepted key, ending in {NULL, NULL}, for reporting misspelt ones. Free with g_free(). */
static struct program_configuration_key *
list_known_keys(void)
{
    GArray *keys = g_array_new(TRUE, TRUE, sizeof(struct program_configuration_key));

    g_array_append_vals(keys, special_keys, G_N_ELEMENTS(special_keys));

    for (size_t i = 0; i < SYSTEM_STATUS_LINE_BLOCK_KIND_COUNT; i++) {
        add_block_keys(keys, &block_descriptions[i]);
    }

    return (struct program_configuration_key *)g_array_free(keys, FALSE);
}

static void
read_settings(
    struct system_status_line_settings *settings,
    struct program_configuration_file  *configuration
)
{
    g_autofree struct program_configuration_key *known_keys = list_known_keys();

    program_configuration_file_report_unknown_keys(configuration, known_keys);
    read_block_order(settings, configuration);
    read_appearance(settings, configuration);
    read_block_specific_settings(settings, configuration);

    for (size_t i = 0; i < SYSTEM_STATUS_LINE_BLOCK_KIND_COUNT; i++) {
        read_block(&settings->blocks[i], &block_descriptions[i], configuration);
    }
}

struct system_status_line_settings *
system_status_line_settings_load(
    const char *override_path
)
{
    struct program_configuration_file  *configuration = program_configuration_file_load("system-status-line", override_path);
    struct system_status_line_settings *settings      = g_new0(struct system_status_line_settings, 1);

    read_settings(settings, configuration);
    program_configuration_file_destroy(configuration);

    return settings;
}

static void
free_block(
    struct system_status_line_block_settings *block
)
{
    g_clear_pointer(&block->formats, g_hash_table_unref);

    for (size_t i = 0; i < SYSTEM_STATUS_LINE_MOUSE_SLOT_COUNT; i++) {
        g_free(block->mouse_actions[i]);
    }
}

static void
free_notification_colors(
    struct system_status_line_settings *settings
)
{
    g_free(settings->quiet_color);
    g_free(settings->unread_color);
    g_free(settings->critical_color);
}

static void
free_texts(
    struct system_status_line_settings *settings
)
{
    g_free(settings->text_color);
    g_free(settings->warning_color);
    g_free(settings->accent_color);
    g_free(settings->accent_text_color);
    free_notification_colors(settings);
    g_free(settings->disk_path);
    g_free(settings->temperature_sensor_name);
}

void
system_status_line_settings_destroy(
    struct system_status_line_settings *settings
)
{
    if (settings == NULL) {
        return;
    }

    for (size_t i = 0; i < SYSTEM_STATUS_LINE_BLOCK_KIND_COUNT; i++) {
        free_block(&settings->blocks[i]);
    }

    free_texts(settings);
    g_free(settings);
}

bool
system_status_line_settings_shows(
    const struct system_status_line_settings *settings,
    enum system_status_line_block_kind        kind
)
{
    for (size_t i = 0; i < settings->shown_block_count; i++) {
        if (settings->shown_blocks[i] == kind) {
            return true;
        }
    }

    return false;
}

const char *
system_status_line_settings_format(
    const struct system_status_line_settings *settings,
    enum system_status_line_block_kind        kind,
    const char                               *format_key
)
{
    const char *format = g_hash_table_lookup(settings->blocks[kind].formats, format_key);

    if (format == NULL) {
        return "";
    }

    return format;
}

const char *
system_status_line_settings_clock_format(
    const struct system_status_line_settings *settings,
    bool                                      is_alternate
)
{
    if (is_alternate) {
        return system_status_line_settings_format(settings, SYSTEM_STATUS_LINE_BLOCK_CLOCK, "format-alt");
    }

    return system_status_line_settings_format(settings, SYSTEM_STATUS_LINE_BLOCK_CLOCK, "format");
}
