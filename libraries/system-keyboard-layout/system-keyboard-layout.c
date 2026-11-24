#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <gio/gio.h>

#include "system-keyboard-layout.h"

/* Which locale1 property fills which field, and the sway setting it becomes. */
static const struct {
    const char *property_name;
    const char *sway_setting;
    size_t      field_offset;
} keyboard_fields[] = {
    {"X11Layout",  "xkb_layout",  offsetof(struct system_keyboard_layout, layout) },
    {"X11Model",   "xkb_model",   offsetof(struct system_keyboard_layout, model)  },
    {"X11Variant", "xkb_variant", offsetof(struct system_keyboard_layout, variant)},
    {"X11Options", "xkb_options", offsetof(struct system_keyboard_layout, options)},
};

static char **
field_at(
    const struct system_keyboard_layout *layout,
    size_t                               field_offset
)
{
    return (char **)((const char *)layout + field_offset);
}

static GDBusProxy *
connect_to_localed(void)
{
    return g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
        NULL,
        "org.freedesktop.locale1",
        "/org/freedesktop/locale1",
        "org.freedesktop.locale1",
        NULL,
        NULL
    );
}

/* An empty or non-text value means unset. */
static char *
copy_nonempty_string(
    GVariant *value
)
{
    if (!g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
        return NULL;
    }

    const char *text = g_variant_get_string(value, NULL);

    if (text[0] == '\0') {
        return NULL;
    }

    return g_strdup(text);
}

static char *
read_property(
    GDBusProxy *localed,
    const char *property_name
)
{
    g_autoptr(GVariant) value = g_dbus_proxy_get_cached_property(localed, property_name);

    if (value == NULL) {
        return NULL;
    }

    return copy_nonempty_string(value);
}

struct system_keyboard_layout *
system_keyboard_layout_read(void)
{
    g_autoptr(GDBusProxy) localed = connect_to_localed();

    if (localed == NULL) {
        return NULL;
    }

    struct system_keyboard_layout *layout = g_new0(struct system_keyboard_layout, 1);

    for (size_t i = 0; i < G_N_ELEMENTS(keyboard_fields); i++) {
        *field_at(layout, keyboard_fields[i].field_offset) = read_property(localed, keyboard_fields[i].property_name);
    }

    return layout;
}

void
system_keyboard_layout_destroy(
    struct system_keyboard_layout *layout
)
{
    if (layout == NULL) {
        return;
    }

    for (size_t i = 0; i < G_N_ELEMENTS(keyboard_fields); i++) {
        g_free(*field_at(layout, keyboard_fields[i].field_offset));
    }

    g_free(layout);
}

/* XKB names are letters, digits and a few punctuation marks; anything else could inject a sway command. */
static bool
is_allowed_character(
    char character
)
{
    if (g_ascii_isalnum(character)) {
        return true;
    }

    return strchr("_-+:,()", character) != NULL;
}

static bool
is_safe_xkb_value(
    const char *value
)
{
    for (const char *character = value; *character != '\0'; character++) {
        if (!is_allowed_character(*character)) {
            return false;
        }
    }

    return true;
}

static bool
append_setting(
    GString    *command,
    const char *sway_setting,
    const char *value
)
{
    if (value == NULL) {
        return true;
    }

    if (!is_safe_xkb_value(value)) {
        return false;
    }

    g_string_append_printf(command, " %s %s", sway_setting, value);

    return true;
}

static bool
append_all_settings(
    GString                             *command,
    const struct system_keyboard_layout *layout
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(keyboard_fields); i++) {
        if (!append_setting(command, keyboard_fields[i].sway_setting, *field_at(layout, keyboard_fields[i].field_offset))) {
            return false;
        }
    }

    return true;
}

char *
system_keyboard_layout_to_compositor_command(
    const struct system_keyboard_layout *layout
)
{
    if (layout->layout == NULL) {
        return NULL;
    }

    GString *command = g_string_new("input type:keyboard");

    if (!append_all_settings(command, layout)) {
        g_string_free(command, TRUE);
        return NULL;
    }

    return g_string_free(command, FALSE);
}
