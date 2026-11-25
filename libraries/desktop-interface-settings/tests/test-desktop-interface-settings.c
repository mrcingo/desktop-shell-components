#include <stdbool.h>
#include <stdlib.h>

#include <gio/gio.h>

#include "desktop-interface-settings.h"

static bool
interface_schema_is_installed(void)
{
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();

    if (source == NULL) {
        return false;
    }

    g_autoptr(GSettingsSchema) schema = g_settings_schema_source_lookup(source, "org.gnome.desktop.interface", TRUE);

    return schema != NULL;
}

/* Runs only where the schema is installed; the memory backend keeps the user's real settings untouched. */
static void
test_values_are_set(void)
{
    if (!interface_schema_is_installed()) {
        g_test_skip("org.gnome.desktop.interface is not installed");
        return;
    }

    g_assert_true(desktop_interface_settings_apply(true, "Monaspace Neon 10"));

    g_autoptr(GSettings) settings     = g_settings_new("org.gnome.desktop.interface");
    g_autofree char     *color_scheme = g_settings_get_string(settings, "color-scheme");

    g_assert_cmpstr(color_scheme, ==, "prefer-dark");
}

int
main(
    int   argc,
    char *argv[]
)
{
    g_setenv("GSETTINGS_BACKEND", "memory", TRUE);
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/desktop-interface-settings/values", test_values_are_set);

    return g_test_run();
}
