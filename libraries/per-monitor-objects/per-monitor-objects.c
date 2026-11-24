#include "per-monitor-objects.h"

struct per_monitor_objects {
    const struct per_monitor_objects_handler *handler;
    void                                     *user_data;

    GListModel *monitors; // owned by the display
    gulong      monitors_changed_handler;

    GPtrArray *objects; // in the order of monitors
};

static void
destroy_object_at(
    struct per_monitor_objects *objects,
    guint                       position
)
{
    void *object = g_ptr_array_steal_index(objects->objects, position);

    objects->handler->destroy_object(objects->user_data, object);
}

static void
create_object_at(
    struct per_monitor_objects *objects,
    guint                       position
)
{
    GdkMonitor *monitor = g_list_model_get_item(objects->monitors, position);
    void       *object  = objects->handler->create_object_for_monitor(objects->user_data, monitor);

    g_ptr_array_insert(objects->objects, (gint)position, object);
    g_object_unref(monitor);
}

static void
handle_monitors_changed(
    GListModel *monitors,
    guint       position,
    guint       removed_count,
    guint       added_count,
    void       *user_data
)
{
    struct per_monitor_objects *objects = user_data;

    for (guint i = 0; i < removed_count; i++) {
        destroy_object_at(objects, position);
    }

    for (guint i = 0; i < added_count; i++) {
        create_object_at(objects, position + i);
    }

    if (objects->handler->handle_monitors_changed != NULL) {
        objects->handler->handle_monitors_changed(objects->user_data, objects);
    }
}

/* Creates objects for the monitors connected now, then follows changes. */
static void
start_following_monitors(
    struct per_monitor_objects *objects
)
{
    guint     connected_count = g_list_model_get_n_items(objects->monitors);
    GCallback changed_handler = G_CALLBACK(handle_monitors_changed);

    objects->monitors_changed_handler = g_signal_connect(objects->monitors, "items-changed", changed_handler, objects);

    handle_monitors_changed(objects->monitors, 0, 0, connected_count, objects);
}

struct per_monitor_objects *
per_monitor_objects_create(
    GdkDisplay                               *display,
    const struct per_monitor_objects_handler *handler,
    void                                     *user_data
)
{
    struct per_monitor_objects *objects = g_new0(struct per_monitor_objects, 1);

    objects->handler   = handler;
    objects->user_data = user_data;
    objects->monitors  = gdk_display_get_monitors(display);
    objects->objects   = g_ptr_array_new();

    start_following_monitors(objects);

    return objects;
}

void
per_monitor_objects_destroy(
    struct per_monitor_objects *objects
)
{
    if (objects == NULL) {
        return;
    }

    g_signal_handler_disconnect(objects->monitors, objects->monitors_changed_handler);

    while (objects->objects->len > 0) {
        destroy_object_at(objects, 0);
    }

    g_ptr_array_unref(objects->objects);
    g_free(objects);
}

guint
per_monitor_objects_count(
    struct per_monitor_objects *objects
)
{
    return objects->objects->len;
}

void *
per_monitor_objects_get_object(
    struct per_monitor_objects *objects,
    guint                       monitor_index
)
{
    g_return_val_if_fail(monitor_index < objects->objects->len, NULL);

    return g_ptr_array_index(objects->objects, monitor_index);
}

/* The display's list keeps its own reference, so the caller needs none. */
static GdkMonitor *
borrow_monitor_at(
    struct per_monitor_objects *objects,
    guint                       monitor_index
)
{
    GdkMonitor *monitor = g_list_model_get_item(objects->monitors, monitor_index);

    g_object_unref(monitor);

    return monitor;
}

GdkMonitor *
per_monitor_objects_find_monitor_by_connector(
    struct per_monitor_objects *objects,
    const char                 *connector_name
)
{
    guint monitor_count = g_list_model_get_n_items(objects->monitors);

    for (guint i = 0; i < monitor_count; i++) {
        GdkMonitor *monitor = borrow_monitor_at(objects, i);

        if (g_strcmp0(gdk_monitor_get_connector(monitor), connector_name) == 0) {
            return monitor;
        }
    }

    return NULL;
}
