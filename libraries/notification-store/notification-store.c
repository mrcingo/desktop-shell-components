#include "notification-store.h"

struct notification_store {
    GPtrArray *notifications; // struct notification_request, newest first
    guint      maximum_count;
    uint32_t   last_id;
};

struct notification_store *
notification_store_create(
    guint maximum_count
)
{
    struct notification_store *store = g_new0(struct notification_store, 1);

    store->notifications = g_ptr_array_new_with_free_func((GDestroyNotify)notification_request_unref);
    store->maximum_count = MAX(maximum_count, 1);

    return store;
}

void
notification_store_destroy(
    struct notification_store *store
)
{
    if (store == NULL) {
        return;
    }

    g_ptr_array_unref(store->notifications);
    g_free(store);
}

/* IDs wrap around but skip 0, which means "no notification" in the specification. */
static uint32_t
next_id(
    struct notification_store *store
)
{
    store->last_id++;

    if (store->last_id == 0) {
        store->last_id = 1;
    }

    return store->last_id;
}

static bool
is_replaced_by(
    const struct notification_request *stored,
    const struct notification_request *notification,
    uint32_t                           replaces_id
)
{
    /* Stored IDs are never 0, so replaces_id 0 matches nothing. */
    if (stored->id == replaces_id) {
        return true;
    }

    if (notification->replacement_tag == NULL) {
        return false;
    }

    return g_strcmp0(stored->replacement_tag, notification->replacement_tag) == 0;
}

static bool
find_replaced_index(
    struct notification_store         *store,
    const struct notification_request *notification,
    uint32_t                           replaces_id,
    guint                             *index
)
{
    for (*index = 0; *index < store->notifications->len; (*index)++) {
        if (is_replaced_by(g_ptr_array_index(store->notifications, *index), notification, replaces_id)) {
            return true;
        }
    }

    return false;
}

/* The ID of the replaced notification, moved to removed, or 0. */
static uint32_t
take_replaced(
    struct notification_store         *store,
    const struct notification_request *notification,
    uint32_t                           replaces_id,
    GPtrArray                         *removed
)
{
    guint index = 0;

    if (!find_replaced_index(store, notification, replaces_id, &index)) {
        return 0;
    }

    struct notification_request *replaced = g_ptr_array_steal_index(store->notifications, index);

    g_ptr_array_add(removed, replaced);

    return replaced->id;
}

static void
drop_oldest_beyond_maximum(
    struct notification_store *store,
    GPtrArray                 *removed
)
{
    while (store->notifications->len > store->maximum_count) {
        g_ptr_array_add(removed, g_ptr_array_steal_index(store->notifications, store->notifications->len - 1));
    }
}

void
notification_store_add(
    struct notification_store   *store,
    struct notification_request *notification,
    uint32_t                     replaces_id,
    GPtrArray                   *removed
)
{
    uint32_t replaced_id = take_replaced(store, notification, replaces_id, removed);

    notification->id        = replaced_id != 0 ? replaced_id : next_id(store);
    notification->is_unread = !notification->is_transient;
    notification->is_open   = true;

    g_ptr_array_insert(store->notifications, 0, notification_request_ref(notification));
    drop_oldest_beyond_maximum(store, removed);
}

static gboolean
has_id(
    gconstpointer stored,
    gconstpointer id
)
{
    return ((const struct notification_request *)stored)->id == *(const uint32_t *)id;
}

struct notification_request *
notification_store_find(
    struct notification_store *store,
    uint32_t                   id
)
{
    guint index = 0;

    if (!g_ptr_array_find_with_equal_func(store->notifications, &id, has_id, &index)) {
        return NULL;
    }

    return g_ptr_array_index(store->notifications, index);
}

struct notification_request *
notification_store_take(
    struct notification_store *store,
    uint32_t                   id
)
{
    guint index = 0;

    if (!g_ptr_array_find_with_equal_func(store->notifications, &id, has_id, &index)) {
        return NULL;
    }

    return g_ptr_array_steal_index(store->notifications, index);
}

guint
notification_store_count_unread(
    struct notification_store *store
)
{
    guint unread_count = 0;

    for (guint i = 0; i < store->notifications->len; i++) {
        const struct notification_request *stored = g_ptr_array_index(store->notifications, i);

        unread_count += stored->is_unread ? 1 : 0;
    }

    return unread_count;
}

struct notification_request *
notification_store_find_latest_unread(
    struct notification_store *store
)
{
    for (guint i = 0; i < store->notifications->len; i++) {
        struct notification_request *stored = g_ptr_array_index(store->notifications, i);

        if (stored->is_unread) {
            return stored;
        }
    }

    return NULL;
}

static void
mark_read(
    struct notification_request *stored,
    GPtrArray                   *newly_read
)
{
    if (!stored->is_unread) {
        return;
    }

    stored->is_unread = false;

    g_ptr_array_add(newly_read, notification_request_ref(stored));
}

void
notification_store_mark_all_read(
    struct notification_store *store,
    GPtrArray                 *newly_read
)
{
    for (guint i = 0; i < store->notifications->len; i++) {
        mark_read(g_ptr_array_index(store->notifications, i), newly_read);
    }
}
