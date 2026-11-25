#ifndef NOTIFICATION_SPECIFICATION_SERVER_H
#define NOTIFICATION_SPECIFICATION_SERVER_H

#include <stdbool.h>
#include <stdint.h>

#include <gio/gio.h>

#include "notification-request.h"

/*
 * The notification server other programs talk to: the Desktop
 * Notifications Specification 1.2 at /org/freedesktop/Notifications under
 * the bus name org.freedesktop.Notifications, and, for the status bar,
 *
 *   object    /io/github/mrcingo/DesktopShellComponents/NotificationCenter
 *   interface io.github.mrcingo.DesktopShellComponents.NotificationCenter
 *     property UnreadCount   u  read
 *     property LatestText    s  read   "Slack: New message", empty when none is unread
 *     property LatestUrgency y  read   0 low, 1 normal, 2 critical
 *     property DoNotDisturb  b  read and write
 *     method   ActivateLatest()
 *     method   MarkAllRead()
 *     method   ToggleDoNotDisturb()
 *
 * Property changes are announced with PropertiesChanged. Only checked
 * notifications reach the handler; malformed calls get an error reply.
 */

enum notification_closed_reason {
    NOTIFICATION_CLOSED_EXPIRED   = 1,
    NOTIFICATION_CLOSED_DISMISSED = 2,
    NOTIFICATION_CLOSED_BY_CALL   = 3,
    NOTIFICATION_CLOSED_UNDEFINED = 4,
};

struct notification_center_state {
    guint                     unread_count;
    const char               *latest_text; // never NULL
    enum notification_urgency latest_urgency;
    bool                      do_not_disturb;
};

struct notification_specification_server_handler {
    /* Takes the notification's reference and returns its ID. */
    uint32_t (*handle_notify)(void *user_data, struct notification_request *notification, uint32_t replaces_id);
    void (*handle_close_notification)(void *user_data, uint32_t id);
    void (*handle_activate_latest)(void *user_data);
    void (*handle_mark_all_read)(void *user_data);
    void (*handle_set_do_not_disturb)(void *user_data, bool do_not_disturb);
    /* Another server owns the name, or the bus is gone. May destroy the server. */
    void (*handle_name_lost)(void *user_data);
};

struct notification_specification_server;

/* connection is the session bus, or a private one in tests. server_version appears in GetServerInformation. */
struct notification_specification_server *notification_specification_server_create(
    GDBusConnection                                        *connection,
    const char                                             *server_version,
    const struct notification_specification_server_handler *handler,
    void                                                   *user_data
);

void notification_specification_server_destroy(struct notification_specification_server *server);

void notification_specification_server_emit_closed(
    struct notification_specification_server *server,
    uint32_t                                  id,
    enum notification_closed_reason           reason
);

void notification_specification_server_emit_action_invoked(
    struct notification_specification_server *server,
    uint32_t                                  id,
    const char                               *action_key
);

/* Announces the properties that differ from the last published state. */
void notification_specification_server_publish_center_state(
    struct notification_specification_server *server,
    const struct notification_center_state   *state
);

/* Calls back with the process ID behind a unique bus name, or 0 when the bus cannot tell. */
typedef void (*notification_sender_process_callback)(int process_id, void *user_data);

void notification_specification_server_find_sender_process(
    struct notification_specification_server *server,
    const char                               *sender_bus_name,
    notification_sender_process_callback      callback,
    void                                     *user_data
);

#endif
