#include <string.h>

#include "notification-request.h"
#include "text-ellipsis-truncation.h"

static const char notify_parameter_type[] = "(susssasa{sv}i)";

/* Hints that make notifications with the same value replace each other, as volume and brightness scripts send. */
static const char *const replacement_tag_hints[] = {"x-canonical-private-synchronous", "x-dunst-stack-tag"};

enum {
    LONGEST_ACTION_KEY = 64,
    LONGEST_TAG        = 128,
};

static void
replace_control_character(
    gunichar *character
)
{
    if (g_unichar_iscntrl(*character)) {
        *character = ' ';
    }
}

static char *
replace_control_characters(
    const char *valid_text
)
{
    GString *cleaned = g_string_new(NULL);

    for (const char *position = valid_text; *position != '\0'; position = g_utf8_next_char(position)) {
        gunichar character = g_utf8_get_char(position);

        replace_control_character(&character);
        g_string_append_unichar(cleaned, character);
    }

    return g_string_free(cleaned, FALSE);
}

char *
notification_request_clean_text(
    const char *text,
    size_t      maximum_characters
)
{
    g_autofree char *valid_text   = g_utf8_make_valid(text, -1);
    g_autofree char *without_cntl = replace_control_characters(valid_text);

    return text_ellipsis_truncate(g_strstrip(without_cntl), maximum_characters);
}

/* NULL when the hint is missing or not a string. */
static char *
read_text_hint(
    GVariant   *hints,
    const char *hint_name,
    size_t      maximum_characters
)
{
    g_autoptr(GVariant) value = g_variant_lookup_value(hints, hint_name, G_VARIANT_TYPE_STRING);

    if (value == NULL) {
        return NULL;
    }

    return notification_request_clean_text(g_variant_get_string(value, NULL), maximum_characters);
}

static bool
read_boolean_hint(
    GVariant   *hints,
    const char *hint_name
)
{
    g_autoptr(GVariant) value = g_variant_lookup_value(hints, hint_name, G_VARIANT_TYPE_BOOLEAN);

    if (value == NULL) {
        return false;
    }

    return g_variant_get_boolean(value);
}

/* Urgencies above critical are treated as critical. */
static enum notification_urgency
read_urgency_hint(
    GVariant *hints
)
{
    g_autoptr(GVariant) value = g_variant_lookup_value(hints, "urgency", G_VARIANT_TYPE_BYTE);

    if (value == NULL) {
        return NOTIFICATION_URGENCY_NORMAL;
    }

    return MIN(g_variant_get_byte(value), NOTIFICATION_URGENCY_CRITICAL);
}

static char *
read_replacement_tag(
    GVariant *hints
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(replacement_tag_hints); i++) {
        char *tag = read_text_hint(hints, replacement_tag_hints[i], LONGEST_TAG);

        if (tag != NULL) {
            return tag;
        }
    }

    return NULL;
}

static void
read_hints(
    struct notification_request *notification,
    GVariant                    *hints
)
{
    notification->urgency          = read_urgency_hint(hints);
    notification->suppresses_sound = read_boolean_hint(hints, "suppress-sound");
    notification->sound_file       = read_text_hint(hints, "sound-file", NOTIFICATION_LONGEST_PATH);
    notification->sound_name       = read_text_hint(hints, "sound-name", NOTIFICATION_LONGEST_TEXT);
    notification->desktop_entry    = read_text_hint(hints, "desktop-entry", NOTIFICATION_LONGEST_TEXT);
    notification->is_transient     = read_boolean_hint(hints, "transient");
    notification->is_resident      = read_boolean_hint(hints, "resident");
}

/* Actions come as key, label, key, label; only the keys matter here. */
static char **
read_action_keys(
    GVariant *actions
)
{
    gsize         pair_count = MIN(g_variant_n_children(actions) / 2, NOTIFICATION_MOST_ACTIONS);
    GStrvBuilder *keys       = g_strv_builder_new();

    for (gsize i = 0; i < pair_count; i++) {
        g_autoptr(GVariant) key = g_variant_get_child_value(actions, 2 * i);

        g_strv_builder_take(keys, notification_request_clean_text(g_variant_get_string(key, NULL), LONGEST_ACTION_KEY));
    }

    return g_strv_builder_unref_to_strv(keys);
}

static void
clear_notification(
    gpointer data
)
{
    struct notification_request *notification = data;

    char *owned_texts[] = {
        notification->sender_bus_name,
        notification->application_name,
        notification->summary,
        notification->desktop_entry,
        notification->sound_file,
        notification->sound_name,
        notification->replacement_tag,
    };

    for (size_t i = 0; i < G_N_ELEMENTS(owned_texts); i++) {
        g_free(owned_texts[i]);
    }

    g_strfreev(notification->action_keys);
}

/* The parts of Notify, borrowed from the parameters. */
struct notify_parameters {
    const char *application_name;
    const char *summary;
    GVariant   *actions;
    GVariant   *hints;
    int32_t     expire_timeout_ms;
};

static struct notification_request *
create_notification(
    const struct notify_parameters *parameters,
    const char                     *sender_bus_name
)
{
    struct notification_request *notification = g_rc_box_new0(struct notification_request);

    *notification = (struct notification_request){
        .sender_bus_name   = g_strdup(sender_bus_name),
        .application_name  = notification_request_clean_text(parameters->application_name, NOTIFICATION_LONGEST_TEXT),
        .summary           = notification_request_clean_text(parameters->summary, NOTIFICATION_LONGEST_TEXT),
        .action_keys       = read_action_keys(parameters->actions),
        .expire_timeout_ms = MAX(parameters->expire_timeout_ms, NOTIFICATION_EXPIRES_AS_SERVER_CHOOSES),
        .replacement_tag   = read_replacement_tag(parameters->hints),
    };

    read_hints(notification, parameters->hints);

    return notification;
}

/* The icon and body are not shown anywhere, so they are not kept. */
static void
unpack_notify_parameters(
    GVariant                 *parameters,
    struct notify_parameters *notify,
    uint32_t                 *replaces_id
)
{
    const char *ignored_icon = NULL;
    const char *ignored_body = NULL;

    g_variant_get(
        parameters,
        "(&su&s&s&s@as@a{sv}i)",
        &notify->application_name,
        replaces_id,
        &ignored_icon,
        &notify->summary,
        &ignored_body,
        &notify->actions,
        &notify->hints,
        &notify->expire_timeout_ms
    );
}

static void
release_notify_parameters(
    struct notify_parameters *notify
)
{
    g_variant_unref(notify->actions);
    g_variant_unref(notify->hints);
}

struct notification_request *
notification_request_parse(
    GVariant   *parameters,
    const char *sender_bus_name,
    uint32_t   *replaces_id
)
{
    struct notify_parameters notify = {0};

    if (!g_variant_is_of_type(parameters, G_VARIANT_TYPE(notify_parameter_type))) {
        return NULL;
    }

    unpack_notify_parameters(parameters, &notify, replaces_id);

    struct notification_request *notification = create_notification(&notify, sender_bus_name);

    release_notify_parameters(&notify);

    return notification;
}

struct notification_request *
notification_request_ref(
    struct notification_request *notification
)
{
    return g_rc_box_acquire(notification);
}

void
notification_request_unref(
    struct notification_request *notification
)
{
    if (notification == NULL) {
        return;
    }

    g_rc_box_release_full(notification, clear_notification);
}

bool
notification_request_has_action(
    const struct notification_request *notification,
    const char                        *action_key
)
{
    return g_strv_contains((const char *const *)notification->action_keys, action_key);
}
