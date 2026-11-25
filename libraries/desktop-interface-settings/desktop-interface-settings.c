#include <gio/gio.h>

#include "desktop-interface-settings.h"

static const char interface_schema_id[] = "org.gnome.desktop.interface";

static GSettingsSchema *
lookup_interface_schema(void)
{
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();

    if (source == NULL) {
        return NULL;
    }

    return g_settings_schema_source_lookup(source, interface_schema_id, TRUE);
}

/* NULL unless the schema is installed and has the color scheme key, which older releases lack. */
static GSettingsSchema *
find_interface_schema(void)
{
    GSettingsSchema *schema = lookup_interface_schema();

    if (schema == NULL) {
        return NULL;
    }

    if (!g_settings_schema_has_key(schema, "color-scheme")) {
        g_settings_schema_unref(schema);
        return NULL;
    }

    return schema;
}

static void
set_string_if_different(
    GSettings  *settings,
    const char *key,
    const char *value
)
{
    g_autofree char *current_value = g_settings_get_string(settings, key);

    if (g_strcmp0(current_value, value) == 0) {
        return;
    }

    g_settings_set_string(settings, key, value);
}

static void
apply_to_schema(
    GSettingsSchema *schema,
    bool             prefers_dark,
    const char      *font_name
)
{
    g_autoptr(GSettings) settings = g_settings_new_full(schema, NULL, NULL);

    set_string_if_different(settings, "color-scheme", prefers_dark ? "prefer-dark" : "prefer-light");
    set_string_if_different(settings, "font-name", font_name);
    g_settings_sync();
}

bool
desktop_interface_settings_apply(
    bool        prefers_dark,
    const char *font_name
)
{
    GSettingsSchema *schema = find_interface_schema();

    if (schema == NULL) {
        return false;
    }

    apply_to_schema(schema, prefers_dark, font_name);
    g_settings_schema_unref(schema);

    return true;
}
