#ifndef NOTIFICATION_STORE_H
#define NOTIFICATION_STORE_H

#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "notification-request.h"

/*
 * The recent notifications, newest first, at most a given number. A new
 * notification replaces an older one with the ID it names, or with the
 * same replacement tag, and keeps that ID; otherwise it gets the next ID,
 * never 0. Transient notifications never count as unread.
 */

struct notification_store;

struct notification_store *notification_store_create(guint maximum_count);

void notification_store_destroy(struct notification_store *store);

/*
 * Stores the notification, taking a reference, and sets its ID. The
 * notification it replaced, or the oldest ones dropped to stay within the
 * maximum, are added to removed (references the caller releases), so their
 * timers can stop.
 */
void notification_store_add(
    struct notification_store   *store,
    struct notification_request *notification,
    uint32_t                     replaces_id,
    GPtrArray                   *removed
);

/* NULL when no notification has the ID. */
struct notification_request *notification_store_find(struct notification_store *store, uint32_t id);

/* Removes and returns the notification (a reference the caller releases), or NULL. */
struct notification_request *notification_store_take(struct notification_store *store, uint32_t id);

guint notification_store_count_unread(struct notification_store *store);

/* The newest unread notification, or NULL. */
struct notification_request *notification_store_find_latest_unread(struct notification_store *store);

/* Adds every notification that was unread to newly_read, as references the caller releases. */
void notification_store_mark_all_read(struct notification_store *store, GPtrArray *newly_read);

#endif
