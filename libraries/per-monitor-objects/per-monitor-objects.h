#ifndef PER_MONITOR_OBJECTS_H
#define PER_MONITOR_OBJECTS_H

#include <gdk/gdk.h>

/*
 * Keeps exactly one object, such as a panel window, for every connected
 * monitor: objects are created for monitors that appear and destroyed for
 * monitors that go away, in the same order as GDK lists the monitors.
 */

struct per_monitor_objects;

struct per_monitor_objects_handler {
    void *(*create_object_for_monitor)(void *user_data, GdkMonitor *monitor);
    void (*destroy_object)(void *user_data, void *object);

    /* Optional: called after every change, including the first creation. */
    void (*handle_monitors_changed)(void *user_data, struct per_monitor_objects *objects);
};

/* Creates objects for the monitors connected now. */
struct per_monitor_objects *
per_monitor_objects_create(GdkDisplay *display, const struct per_monitor_objects_handler *handler, void *user_data);

/* Destroys every object. */
void per_monitor_objects_destroy(struct per_monitor_objects *objects);

guint per_monitor_objects_count(struct per_monitor_objects *objects);

void *per_monitor_objects_get_object(struct per_monitor_objects *objects, guint monitor_index);

/* The monitor whose connector is named connector_name, such as eDP-1. */
GdkMonitor *per_monitor_objects_find_monitor_by_connector(struct per_monitor_objects *objects, const char *connector_name);

#endif
