#include <math.h>

#include <glib.h>

#include "battery-charge-status.h"
#include "system-bus-properties.h"

static const char service_name[]     = "org.freedesktop.UPower";
static const char device_interface[] = "org.freedesktop.UPower.Device";

const char battery_charge_status_object_path[] = "/org/freedesktop/UPower/devices/DisplayDevice";

static int
read_percent(
    GVariant *device_properties
)
{
    g_autoptr(GVariant) percentage = system_bus_dictionary_lookup(device_properties, "Percentage", G_VARIANT_TYPE_DOUBLE);

    if (percentage == NULL) {
        return 0;
    }

    return (int)CLAMP(lround(g_variant_get_double(percentage)), 0, 100);
}

static int64_t
read_seconds(
    GVariant   *device_properties,
    const char *property_name
)
{
    g_autoptr(GVariant) seconds = system_bus_dictionary_lookup(device_properties, property_name, G_VARIANT_TYPE_INT64);

    if (seconds == NULL) {
        return 0;
    }

    return MAX(0, g_variant_get_int64(seconds));
}

static enum battery_charge_state
read_state(
    GVariant *device_properties
)
{
    uint32_t state = 0;

    if (!system_bus_dictionary_lookup_unsigned(device_properties, "State", &state)) {
        return BATTERY_CHARGE_STATE_UNKNOWN;
    }

    if (state > BATTERY_CHARGE_STATE_PENDING_DISCHARGE) {
        return BATTERY_CHARGE_STATE_UNKNOWN;
    }

    return (enum battery_charge_state)state;
}

void
battery_charge_status_parse(
    GVariant                     *device_properties,
    struct battery_charge_status *status
)
{
    *status = (struct battery_charge_status){
        .is_present          = system_bus_dictionary_lookup_boolean(device_properties, "IsPresent", false),
        .percent             = read_percent(device_properties),
        .state               = read_state(device_properties),
        .seconds_until_empty = read_seconds(device_properties, "TimeToEmpty"),
        .seconds_until_full  = read_seconds(device_properties, "TimeToFull"),
    };
}

void
battery_charge_status_read(
    GDBusConnection              *connection,
    struct battery_charge_status *status
)
{
    g_autoptr(GVariant) properties = system_bus_properties_read_all(
        connection,
        service_name,
        battery_charge_status_object_path,
        device_interface
    );

    battery_charge_status_parse(properties, status);
}
