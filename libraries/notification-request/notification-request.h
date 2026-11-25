#ifndef NOTIFICATION_REQUEST_H
#define NOTIFICATION_REQUEST_H

#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

/*
 * One notification as another program asked for it with the
 * Desktop Notifications Specification's Notify call, checked and cleaned:
 * every string is valid UTF-8 without control characters and capped in
 * length, actions come in key and label pairs and are capped in number, and
 * hints of the wrong type are ignored. Notifications are reference counted.
 */

enum notification_urgency {
    NOTIFICATION_URGENCY_LOW      = 0,
    NOTIFICATION_URGENCY_NORMAL   = 1,
    NOTIFICATION_URGENCY_CRITICAL = 2,
};

enum {
    NOTIFICATION_EXPIRES_AS_SERVER_CHOOSES = -1,
    NOTIFICATION_NEVER_EXPIRES             = 0,
};

struct notification_request {
    uint32_t id;              // 0 until stored
    char    *sender_bus_name; // the unique bus name of the sending connection
    char    *application_name;
    char    *summary;
    char    *desktop_entry;     // NULL when not given
    char   **action_keys;       // never NULL; keys of the actions, such as "default"
    int      expire_timeout_ms; // NOTIFICATION_EXPIRES_AS_SERVER_CHOOSES, NOTIFICATION_NEVER_EXPIRES or milliseconds

    enum notification_urgency urgency;

    bool  suppresses_sound;
    char *sound_file; // NULL when not given
    char *sound_name; // NULL when not given
    bool  is_transient;
    bool  is_resident;
    char *replacement_tag; // NULL; notifications with the same tag replace each other

    bool is_unread;
    bool is_open; // not yet expired or closed
};

enum {
    NOTIFICATION_LONGEST_TEXT = 512,
    NOTIFICATION_MOST_ACTIONS = 16,
    NOTIFICATION_LONGEST_PATH = 4096,
};

/*
 * Parses the parameters of Notify, type (susssasa{sv}i). Returns NULL for
 * parameters of another type; the replaces_id parameter is stored in
 * replaces_id.
 */
struct notification_request *
notification_request_parse(GVariant *parameters, const char *sender_bus_name, uint32_t *replaces_id);

struct notification_request *notification_request_ref(struct notification_request *notification);

void notification_request_unref(struct notification_request *notification);

bool notification_request_has_action(const struct notification_request *notification, const char *action_key);

/* Text cleaned the same way: valid UTF-8, control characters as spaces, at most maximum_characters. Free with g_free(). */
char *notification_request_clean_text(const char *text, size_t maximum_characters);

#endif
