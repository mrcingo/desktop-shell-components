#ifndef STATUS_NOTIFIER_ACTIVATION_WATCHER_H
#define STATUS_NOTIFIER_ACTIVATION_WATCHER_H

#include <gio/gio.h>

/*
 * Notices when a tray icon is clicked: a bar calls Activate on the icon's
 * StatusNotifierItem, which this sees as a D-Bus monitor (the standard
 * org.freedesktop.DBus.Monitoring interface, as dbus-monitor uses) and
 * reports with the process ID of the application that owns the icon. Only
 * those calls are looked at; nothing on the bus is changed or answered.
 */

struct status_notifier_activation_watcher_handler {
    /* Runs in the thread default main context of whoever created the watcher. */
    void (*handle_item_activated)(void *user_data, int process_id);
};

struct status_notifier_activation_watcher;

/* bus_address NULL watches the session bus. Returns NULL with error set when monitoring is refused. */
struct status_notifier_activation_watcher *status_notifier_activation_watcher_create(
    const char                                              *bus_address,
    const struct status_notifier_activation_watcher_handler *handler,
    void                                                    *user_data,
    GError                                                 **error
);

void status_notifier_activation_watcher_destroy(struct status_notifier_activation_watcher *watcher);

#endif
