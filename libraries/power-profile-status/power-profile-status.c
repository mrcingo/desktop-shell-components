#include <glib.h>

#include "power-profile-status.h"
#include "system-bus-properties.h"

static const char service_name[] = "net.hadess.PowerProfiles";
static const char object_path[]  = "/net/hadess/PowerProfiles";

static const char *const profile_order[] = {"power-saver", "balanced", "performance"};

enum {
    PROFILE_COUNT = G_N_ELEMENTS(profile_order),
};

static void
add_profile_name(
    GPtrArray *names,
    GVariant  *profile
)
{
    char *name = system_bus_dictionary_lookup_text(profile, "Profile");

    if (name != NULL) {
        g_ptr_array_add(names, name);
    }
}

/* Entries without a name are left out. */
static void
add_profile_names(
    GPtrArray *names,
    GVariant  *profiles
)
{
    for (gsize i = 0; i < g_variant_n_children(profiles); i++) {
        g_autoptr(GVariant) profile = g_variant_get_child_value(profiles, i);

        add_profile_name(names, profile);
    }
}

/* Each entry of Profiles is a dictionary with the profile's name under "Profile". */
static char **
read_available_profiles(
    GVariant *properties
)
{
    g_autoptr(GVariant) profiles = system_bus_dictionary_lookup(properties, "Profiles", G_VARIANT_TYPE("aa{sv}"));

    GPtrArray *names = g_ptr_array_new();

    if (profiles != NULL) {
        add_profile_names(names, profiles);
    }

    g_ptr_array_add(names, NULL);

    return (char **)g_ptr_array_free(names, FALSE);
}

void
power_profile_status_read(
    GDBusConnection             *connection,
    struct power_profile_status *status
)
{
    g_autoptr(GVariant) properties = system_bus_properties_read_all(connection, service_name, object_path, service_name);

    power_profile_status_clear(status);

    if (properties == NULL) {
        return;
    }

    status->active_profile     = system_bus_dictionary_lookup_text(properties, "ActiveProfile");
    status->available_profiles = read_available_profiles(properties);
}

void
power_profile_status_clear(
    struct power_profile_status *status
)
{
    g_free(status->active_profile);
    g_strfreev(status->available_profiles);

    *status = (struct power_profile_status){0};
}

static size_t
position_in_order(
    const char *profile
)
{
    for (size_t i = 0; i < PROFILE_COUNT; i++) {
        if (g_strcmp0(profile, profile_order[i]) == 0) {
            return i;
        }
    }

    return PROFILE_COUNT - 1; // an unknown profile goes on to the first one
}

static bool
can_switch_to(
    const char        *candidate,
    const char        *current_profile,
    const char *const *available_profiles
)
{
    if (g_strcmp0(candidate, current_profile) == 0) {
        return false;
    }

    return g_strv_contains(available_profiles, candidate);
}

static const char *
first_switchable_after(
    size_t             current_position,
    const char        *current_profile,
    const char *const *available_profiles
)
{
    for (size_t step = 1; step <= PROFILE_COUNT; step++) {
        const char *candidate = profile_order[(current_position + step) % PROFILE_COUNT];

        if (can_switch_to(candidate, current_profile, available_profiles)) {
            return candidate;
        }
    }

    return NULL;
}

const char *
power_profile_following(
    const char        *current_profile,
    const char *const *available_profiles
)
{
    if (available_profiles == NULL) {
        return NULL;
    }

    return first_switchable_after(position_in_order(current_profile), current_profile, available_profiles);
}

void
power_profile_activate(
    GDBusConnection *connection,
    const char      *profile
)
{
    system_bus_property_write(
        connection,
        service_name,
        object_path,
        service_name,
        "ActiveProfile",
        g_variant_new_string(profile)
    );
}
