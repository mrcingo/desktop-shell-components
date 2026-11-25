#ifndef NOTIFICATION_CENTER_STATUS_H
#define NOTIFICATION_CENTER_STATUS_H

#include <stdbool.h>
#include <stdint.h>

/*
 * The state of the desktop-shell notification daemon (unread notifications,
 * the latest one's text and urgency, do-not-disturb), followed on the
 * session bus as it changes, and the commands it accepts. Another
 * notification daemon owning the name, or none at all, reads as
 * unavailable; the daemon is picked up when it starts.
 */

enum notification_urgency {
    NOTIFICATION_URGENCY_LOW,
    NOTIFICATION_URGENCY_NORMAL,
    NOTIFICATION_URGENCY_CRITICAL,
};

struct notification_center_state {
    bool                      is_known; // false until the session bus has answered
    bool                      is_available;
    uint32_t                  unread_count;
    char                     *latest_text; // plain text, never NULL while available
    enum notification_urgency latest_urgency;
    bool                      does_not_disturb;
};

struct notification_center_status_handler {
    /* Only called when the state differs from the last one reported. */
    void (*handle_state_changed)(void *user_data, const struct notification_center_state *state);
};

struct notification_center_status;

/* Runs on the default GLib main context. */
struct notification_center_status *
notification_center_status_create(const struct notification_center_status_handler *handler, void *user_data);

void notification_center_status_destroy(struct notification_center_status *status);

const struct notification_center_state *notification_center_status_get_state(const struct notification_center_status *status);

/* Each does nothing while the daemon is unavailable. */
void notification_center_activate_latest(struct notification_center_status *status); // opens the sending application
void notification_center_mark_all_read(struct notification_center_status *status);
void notification_center_toggle_do_not_disturb(struct notification_center_status *status);

#endif
