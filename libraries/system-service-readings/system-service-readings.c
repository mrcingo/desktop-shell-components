#include <glib.h>

#include "system-bus-properties.h"
#include "system-service-readings.h"

struct system_service_readings;

/* One service: its D-Bus name, how to read it and which of its signals matter. */
struct service_description {
    enum system_service_reading reading;
    const char                 *service_name;
    void (*read)(struct system_service_readings *readings);
    bool (*signal_matters)(const struct system_service_readings *readings, const char *object_path);
};

struct system_service_readings {
    unsigned                                      enabled_readings;
    unsigned                                      readings_to_refresh; // flags waiting for the idle refresh
    GDBusConnection                              *connection;          // NULL without a system bus
    GPtrArray                                    *watches;             // struct system_bus_service_watch
    guint                                         refresh_source;
    const struct system_service_readings_handler *handler;
    void                                         *user_data;
    struct system_service_values                  values;
};

/* A signal watch remembers which service it belongs to. */
struct service_watch_context {
    struct system_service_readings   *readings;
    const struct service_description *service;
};

static void
read_network(
    struct system_service_readings *readings
)
{
    network_manager_status_read(readings->connection, &readings->values.network);
}

static void
read_bluetooth(
    struct system_service_readings *readings
)
{
    bluetooth_adapter_status_read(readings->connection, &readings->values.bluetooth);
}

static void
read_power_profile(
    struct system_service_readings *readings
)
{
    power_profile_status_read(readings->connection, &readings->values.power_profile);
}

static void
read_battery(
    struct system_service_readings *readings
)
{
    battery_charge_status_read(readings->connection, &readings->values.battery);
}

/* Scans report every access point nearby; only the objects in use matter. */
static bool
network_signal_matters(
    const struct system_service_readings *readings,
    const char                           *object_path
)
{
    return network_manager_status_concerns_object(&readings->values.network, object_path);
}

/* UPower also reports mice and headsets; only the combined battery matters. */
static bool
battery_signal_matters(
    const struct system_service_readings *readings,
    const char                           *object_path
)
{
    if (object_path == NULL) {
        return true;
    }

    return g_strcmp0(object_path, battery_charge_status_object_path) == 0;
}

static bool
every_signal_matters(
    const struct system_service_readings *readings,
    const char                           *object_path
)
{
    return true;
}

static const struct service_description services[] = {
    {SYSTEM_SERVICE_READING_NETWORK,       "org.freedesktop.NetworkManager", read_network,       network_signal_matters},
    {SYSTEM_SERVICE_READING_BLUETOOTH,     "org.bluez",                      read_bluetooth,     every_signal_matters  },
    {SYSTEM_SERVICE_READING_POWER_PROFILE, "net.hadess.PowerProfiles",       read_power_profile, every_signal_matters  },
    {SYSTEM_SERVICE_READING_BATTERY,       "org.freedesktop.UPower",         read_battery,       battery_signal_matters},
};

static void
read_marked_services(
    struct system_service_readings *readings,
    unsigned                        marked_readings
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(services); i++) {
        if ((marked_readings & services[i].reading) != 0) {
            services[i].read(readings);
        }
    }
}

static gboolean
refresh_marked_services(
    void *user_data
)
{
    struct system_service_readings *readings = user_data;

    readings->refresh_source = 0;

    read_marked_services(readings, readings->readings_to_refresh);

    readings->readings_to_refresh = 0;

    readings->handler->handle_values_changed(readings->user_data, &readings->values);

    return G_SOURCE_REMOVE;
}

/* Signals often come in bursts; the idle refresh reads each service once for all of them. */
static void
handle_service_signal(
    void       *user_data,
    const char *object_path
)
{
    struct service_watch_context *context = user_data;

    if (!context->service->signal_matters(context->readings, object_path)) {
        return;
    }

    context->readings->readings_to_refresh |= context->service->reading;

    if (context->readings->refresh_source == 0) {
        context->readings->refresh_source = g_idle_add(refresh_marked_services, context->readings);
    }
}

struct watch_with_context {
    struct system_bus_service_watch *watch;
    struct service_watch_context     context;
};

static void
destroy_watch_with_context(
    void *data
)
{
    struct watch_with_context *watch = data;

    system_bus_service_watch_destroy(watch->watch);
    g_free(watch);
}

static void
watch_service(
    struct system_service_readings   *readings,
    const struct service_description *service
)
{
    struct watch_with_context *watch = g_new0(struct watch_with_context, 1);

    watch->context = (struct service_watch_context){.readings = readings, .service = service};
    watch->watch   = system_bus_service_watch_create(
        readings->connection,
        service->service_name,
        handle_service_signal,
        &watch->context
    );

    g_ptr_array_add(readings->watches, watch);
}

static void
watch_enabled_services(
    struct system_service_readings *readings
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(services); i++) {
        if ((readings->enabled_readings & services[i].reading) != 0) {
            watch_service(readings, &services[i]);
        }
    }
}

static void
clear_values(
    struct system_service_values *values
)
{
    network_manager_status_clear(&values->network);
    bluetooth_adapter_status_clear(&values->bluetooth);
    power_profile_status_clear(&values->power_profile);

    values->battery = (struct battery_charge_status){0};
}

struct system_service_readings *
system_service_readings_create(
    unsigned                                      enabled_readings,
    const struct system_service_readings_handler *handler,
    void                                         *user_data
)
{
    struct system_service_readings *readings = g_new0(struct system_service_readings, 1);

    *readings = (struct system_service_readings){
        .enabled_readings = enabled_readings,
        .connection       = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL),
        .watches          = g_ptr_array_new_with_free_func(destroy_watch_with_context),
        .handler          = handler,
        .user_data        = user_data,
    };

    clear_values(&readings->values);

    if (readings->connection != NULL) {
        watch_enabled_services(readings);
        read_marked_services(readings, enabled_readings);
    }

    return readings;
}

void
system_service_readings_destroy(
    struct system_service_readings *readings
)
{
    if (readings == NULL) {
        return;
    }

    g_clear_handle_id(&readings->refresh_source, g_source_remove);
    g_ptr_array_unref(readings->watches);
    g_clear_object(&readings->connection);
    clear_values(&readings->values);
    g_free(readings);
}

const struct system_service_values *
system_service_readings_get_values(
    const struct system_service_readings *readings
)
{
    return &readings->values;
}

void
system_service_readings_toggle_bluetooth(
    struct system_service_readings *readings
)
{
    const struct bluetooth_adapter_status *bluetooth = &readings->values.bluetooth;

    if (readings->connection == NULL) {
        return;
    }

    bluetooth_adapter_set_powered(readings->connection, bluetooth, !bluetooth->is_powered);
}

void
system_service_readings_switch_power_profile(
    struct system_service_readings *readings
)
{
    const struct power_profile_status *status            = &readings->values.power_profile;
    const char                        *following_profile = power_profile_following(
        status->active_profile,
        (const char *const *)status->available_profiles
    );

    if (readings->connection == NULL) {
        return;
    }

    if (following_profile == NULL) {
        return;
    }

    power_profile_activate(readings->connection, following_profile);
}
