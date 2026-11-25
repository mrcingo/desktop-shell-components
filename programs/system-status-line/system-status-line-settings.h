#ifndef SYSTEM_STATUS_LINE_SETTINGS_H
#define SYSTEM_STATUS_LINE_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#include <glib.h>

/*
 * Settings from system-status-line.ini: which blocks to show and in what
 * order, and per block its formats with {placeholders}, warning and urgent
 * thresholds, mouse actions and, for values read on a timer, how often.
 * Every one is optional; the shipped system-status-line.ini lists them
 * all with their defaults.
 */

enum system_status_line_block_kind {
    SYSTEM_STATUS_LINE_BLOCK_NOTIFICATIONS,
    SYSTEM_STATUS_LINE_BLOCK_ATTENTION,
    SYSTEM_STATUS_LINE_BLOCK_NETWORK,
    SYSTEM_STATUS_LINE_BLOCK_TRAFFIC,
    SYSTEM_STATUS_LINE_BLOCK_BLUETOOTH,
    SYSTEM_STATUS_LINE_BLOCK_PROCESSOR,
    SYSTEM_STATUS_LINE_BLOCK_TEMPERATURE,
    SYSTEM_STATUS_LINE_BLOCK_MEMORY,
    SYSTEM_STATUS_LINE_BLOCK_DISK,
    SYSTEM_STATUS_LINE_BLOCK_VOLUME,
    SYSTEM_STATUS_LINE_BLOCK_BRIGHTNESS,
    SYSTEM_STATUS_LINE_BLOCK_POWER_PROFILE,
    SYSTEM_STATUS_LINE_BLOCK_BATTERY,
    SYSTEM_STATUS_LINE_BLOCK_KEYBOARD_LAYOUT,
    SYSTEM_STATUS_LINE_BLOCK_CLOCK,
    SYSTEM_STATUS_LINE_BLOCK_KIND_COUNT,
};

/* Mouse buttons a block can act on, in the order of their settings. */
enum system_status_line_mouse_slot {
    SYSTEM_STATUS_LINE_MOUSE_LEFT,
    SYSTEM_STATUS_LINE_MOUSE_MIDDLE,
    SYSTEM_STATUS_LINE_MOUSE_RIGHT,
    SYSTEM_STATUS_LINE_MOUSE_SCROLL_UP,
    SYSTEM_STATUS_LINE_MOUSE_SCROLL_DOWN,
    SYSTEM_STATUS_LINE_MOUSE_SLOT_COUNT,
};

struct system_status_line_block_settings {
    GHashTable *formats;                                            // "format", "format-muted", ... -> text
    char       *mouse_actions[SYSTEM_STATUS_LINE_MOUSE_SLOT_COUNT]; // built-in action or command; "" for nothing
    int         warning_threshold;                                  // -1 for none; see the shipped settings for each block
    int         urgent_threshold;                                   // -1 for none
    unsigned    interval_seconds;                                   // blocks read on a timer only
};

struct system_status_line_settings {
    enum system_status_line_block_kind       shown_blocks[SYSTEM_STATUS_LINE_BLOCK_KIND_COUNT]; // left to right
    size_t                                   shown_block_count;
    struct system_status_line_block_settings blocks[SYSTEM_STATUS_LINE_BLOCK_KIND_COUNT];

    char *text_color;        // "#rrggbb", values in their normal state
    char *warning_color;     // values past their warning threshold
    char *accent_color;      // background of blocks that want a look, such as unread notifications
    char *accent_text_color; // their text
    char *quiet_color;       // markers that only hold a place, such as "0" notifications
    char *unread_color;      // unread notifications after their first seconds
    char *critical_color;    // unread critical notifications after their first seconds
    int   separator_width;
    bool  pads_numbers; // numbers keep a fixed width so blocks do not move

    int   notification_width;                // characters of the latest notification shown
    int   new_notification_emphasis_seconds; // 0: a new notification is not emphasized
    bool  shows_empty_notifications;         // the quiet marker while nothing is unread
    char *disk_path;                         // the filesystem whose space is shown
    char *temperature_sensor_name;           // hwmon driver name
    int   volume_step_percent;
    int   brightness_step_percent;
};

/* override_path is the --config file, or NULL. */
struct system_status_line_settings *system_status_line_settings_load(const char *override_path);

void system_status_line_settings_destroy(struct system_status_line_settings *settings);

bool
system_status_line_settings_shows(const struct system_status_line_settings *settings, enum system_status_line_block_kind kind);

/* The block's format for format_key; "" when it has none. */
const char *system_status_line_settings_format(
    const struct system_status_line_settings *settings,
    enum system_status_line_block_kind        kind,
    const char                               *format_key
);

/* The clock's strftime-style format, normal or alternate. */
const char *system_status_line_settings_clock_format(const struct system_status_line_settings *settings, bool is_alternate);

/* The name a block has in the settings and in click events, such as "network". */
const char *system_status_line_block_name(enum system_status_line_block_kind kind);

#endif
