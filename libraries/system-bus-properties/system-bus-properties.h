#ifndef SYSTEM_BUS_PROPERTIES_H
#define SYSTEM_BUS_PROPERTIES_H

#include <stdbool.h>
#include <stdint.h>

#include <gio/gio.h>

/*
 * Small, typed access to the properties of system services on D-Bus, such
 * as NetworkManager or UPower. Replies come from other processes: a value
 * that is missing or of another type reads as absent, never as a crash or
 * a critical warning. Reads are synchronous with a short timeout and meant
 * for a few properties at a time; services are never started by a read.
 */

/* Every property of an interface as a{sv}, or NULL. Unref with g_variant_unref(). */
GVariant *system_bus_properties_read_all(
    GDBusConnection *connection,
    const char      *service_name,
    const char      *object_path,
    const char      *interface_name
);

/* Calls a method without arguments; NULL unless the reply has reply_type. */
GVariant *system_bus_call_without_arguments(
    GDBusConnection    *connection,
    const char         *service_name,
    const char         *object_path,
    const char         *interface_name,
    const char         *method_name,
    const GVariantType *reply_type
);

/* Sets a property without waiting; a refusal is logged once as a message. Takes a floating value. */
void system_bus_property_write(
    GDBusConnection *connection,
    const char      *service_name,
    const char      *object_path,
    const char      *interface_name,
    const char      *property_name,
    GVariant        *value
);

/* Typed members of an a{sv} dictionary; a NULL dictionary reads as empty. */
GVariant *system_bus_dictionary_lookup(GVariant *dictionary, const char *key, const GVariantType *expected_type);
char     *system_bus_dictionary_lookup_text(GVariant *dictionary, const char *key); // s or o; free with g_free()
bool      system_bus_dictionary_lookup_boolean(GVariant *dictionary, const char *key, bool fallback_value);
bool      system_bus_dictionary_lookup_unsigned(GVariant *dictionary, const char *key, uint32_t *value);

/*
 * Reports every signal a service sends (property changes, objects added or
 * removed) with the object it concerns, so a reader can refresh. A service
 * that starts, stops or restarts is reported with object_path NULL, meaning
 * that anything may have changed. Callbacks must not destroy the watch.
 */
struct system_bus_service_watch;

struct system_bus_service_watch *system_bus_service_watch_create(
    GDBusConnection *connection,
    const char      *service_name,
    void (*handle_signal)(void *user_data, const char *object_path),
    void *user_data
);

void system_bus_service_watch_destroy(struct system_bus_service_watch *watch);

#endif
