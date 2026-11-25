#include <glib.h>

#include "bluetooth-adapter-status.h"
#include "system-bus-properties.h"

static const char service_name[]      = "org.bluez";
static const char adapter_interface[] = "org.bluez.Adapter1";
static const char device_interface[]  = "org.bluez.Device1";

/* The first adapter found is the one shown. */
static void
read_adapter(
    const char                      *object_path,
    GVariant                        *adapter_properties,
    struct bluetooth_adapter_status *status
)
{
    if (status->adapter_is_present) {
        return;
    }

    status->adapter_is_present = true;
    status->is_powered         = system_bus_dictionary_lookup_boolean(adapter_properties, "Powered", false);
    status->adapter_path       = g_strdup(object_path);
}

static void
read_object(
    const char                      *object_path,
    GVariant                        *interfaces,
    struct bluetooth_adapter_status *status
)
{
    g_autoptr(GVariant) adapter_properties = g_variant_lookup_value(interfaces, adapter_interface, G_VARIANT_TYPE_VARDICT);
    g_autoptr(GVariant) device_properties  = g_variant_lookup_value(interfaces, device_interface, G_VARIANT_TYPE_VARDICT);

    if (adapter_properties != NULL) {
        read_adapter(object_path, adapter_properties, status);
    }

    if (system_bus_dictionary_lookup_boolean(device_properties, "Connected", false)) {
        status->connected_device_count++;
    }
}

static void
read_objects(
    GVariant                        *objects,
    struct bluetooth_adapter_status *status
)
{
    GVariantIter iterator;
    const char  *object_path = NULL;
    GVariant    *interfaces  = NULL;

    g_variant_iter_init(&iterator, objects);

    while (g_variant_iter_loop(&iterator, "{&o@a{sa{sv}}}", &object_path, &interfaces)) {
        read_object(object_path, interfaces, status);
    }
}

static bool
is_managed_objects_reply(
    GVariant *reply
)
{
    if (reply == NULL) {
        return false;
    }

    return g_variant_is_of_type(reply, G_VARIANT_TYPE("(a{oa{sa{sv}}})"));
}

void
bluetooth_adapter_status_parse(
    GVariant                        *managed_objects_reply,
    struct bluetooth_adapter_status *status
)
{
    bluetooth_adapter_status_clear(status);

    if (!is_managed_objects_reply(managed_objects_reply)) {
        return;
    }

    g_autoptr(GVariant) objects = g_variant_get_child_value(managed_objects_reply, 0);

    read_objects(objects, status);
}

void
bluetooth_adapter_status_read(
    GDBusConnection                 *connection,
    struct bluetooth_adapter_status *status
)
{
    g_autoptr(GVariant) reply = system_bus_call_without_arguments(
        connection,
        service_name,
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects",
        G_VARIANT_TYPE("(a{oa{sa{sv}}})")
    );

    bluetooth_adapter_status_parse(reply, status);
}

void
bluetooth_adapter_status_clear(
    struct bluetooth_adapter_status *status
)
{
    g_free(status->adapter_path);

    *status = (struct bluetooth_adapter_status){0};
}

void
bluetooth_adapter_set_powered(
    GDBusConnection                       *connection,
    const struct bluetooth_adapter_status *status,
    bool                                   is_powered
)
{
    if (status->adapter_path == NULL) {
        return;
    }

    system_bus_property_write(
        connection,
        service_name,
        status->adapter_path,
        adapter_interface,
        "Powered",
        g_variant_new_boolean(is_powered)
    );
}
