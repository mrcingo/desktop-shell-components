#include <glib.h>

#include "compositor-ipc-client.h"
#include "compositor-keyboard-layout-follower.h"
#include "compositor-keyboard-layout.h"
#include "keyboard-layout-short-names.h"

static const char subscribed_event_names[] = "[\"input\"]";

struct compositor_keyboard_layout_follower {
    struct compositor_ipc_client                             *ipc_client;
    char                                                     *rules_file_path;
    const struct compositor_keyboard_layout_follower_handler *handler;
    void                                                     *user_data;

    char *layout_description; // the compositor's name, to skip repeated lookups
    char *layout_name;
};

static bool
is_new_layout(
    const struct compositor_keyboard_layout_follower *follower,
    const char                                       *layout_description
)
{
    if (layout_description == NULL) {
        return false;
    }

    return g_strcmp0(layout_description, follower->layout_description) != 0;
}

/* Takes ownership of layout_description; false when it is missing or unchanged. */
static bool
adopt_layout(
    struct compositor_keyboard_layout_follower *follower,
    char                                       *layout_description
)
{
    if (!is_new_layout(follower, layout_description)) {
        g_free(layout_description);
        return false;
    }

    g_free(follower->layout_description);

    follower->layout_description = layout_description;

    return true;
}

static char *
name_layout(
    const struct compositor_keyboard_layout_follower *follower
)
{
    char *short_name = keyboard_layout_short_name_find(follower->rules_file_path, follower->layout_description);

    if (short_name == NULL) {
        return g_strdup(follower->layout_description);
    }

    return short_name;
}

static void
report_layout_if_new(
    struct compositor_keyboard_layout_follower *follower,
    char                                       *layout_description
)
{
    if (!adopt_layout(follower, layout_description)) {
        return;
    }

    g_free(follower->layout_name);

    follower->layout_name = name_layout(follower);

    follower->handler->handle_layout_changed(follower->user_data, follower->layout_name);
}

static void
handle_inputs_reply(
    JsonNode *reply_payload,
    void     *user_data
)
{
    report_layout_if_new(user_data, compositor_keyboard_layout_from_inputs(reply_payload));
}

static void
handle_event(
    void     *user_data,
    uint32_t  event_type,
    JsonNode *event_payload
)
{
    if (event_type != COMPOSITOR_IPC_EVENT_INPUT_CHANGED) {
        return;
    }

    report_layout_if_new(user_data, compositor_keyboard_layout_from_input_event(event_payload));
}

/* Nothing to do: the last layout stays shown until the compositor returns with a new session. */
static void
handle_connection_lost(
    void *user_data
)
{}

static const struct compositor_ipc_client_handler ipc_client_handler = {
    .handle_event           = handle_event,
    .handle_connection_lost = handle_connection_lost,
};

static void
start_following(
    struct compositor_keyboard_layout_follower *follower
)
{
    compositor_ipc_client_send_request(
        follower->ipc_client,
        COMPOSITOR_IPC_REQUEST_SUBSCRIBE_TO_EVENTS,
        subscribed_event_names,
        NULL,
        NULL
    );
    compositor_ipc_client_send_request(
        follower->ipc_client,
        COMPOSITOR_IPC_REQUEST_GET_INPUTS,
        NULL,
        handle_inputs_reply,
        follower
    );
}

static struct compositor_keyboard_layout_follower *
create_follower(
    const char                                               *rules_file_path,
    const struct compositor_keyboard_layout_follower_handler *handler,
    void                                                     *user_data
)
{
    struct compositor_keyboard_layout_follower *follower = g_new0(struct compositor_keyboard_layout_follower, 1);

    follower->rules_file_path = g_strdup(rules_file_path);
    follower->handler         = handler;
    follower->user_data       = user_data;

    return follower;
}

struct compositor_keyboard_layout_follower *
compositor_keyboard_layout_follower_connect(
    const char                                               *socket_path,
    const char                                               *rules_file_path,
    const struct compositor_keyboard_layout_follower_handler *handler,
    void                                                     *user_data,
    GError                                                  **error
)
{
    struct compositor_keyboard_layout_follower *follower = create_follower(rules_file_path, handler, user_data);

    follower->ipc_client = compositor_ipc_client_connect(socket_path, &ipc_client_handler, follower, error);

    if (follower->ipc_client == NULL) {
        compositor_keyboard_layout_follower_destroy(follower);
        return NULL;
    }

    start_following(follower);

    return follower;
}

struct compositor_keyboard_layout_follower *
compositor_keyboard_layout_follower_create_for_connection(
    GSocketConnection                                        *connection,
    const char                                               *rules_file_path,
    const struct compositor_keyboard_layout_follower_handler *handler,
    void                                                     *user_data
)
{
    struct compositor_keyboard_layout_follower *follower = create_follower(rules_file_path, handler, user_data);

    follower->ipc_client = compositor_ipc_client_create_for_connection(connection, &ipc_client_handler, follower);

    start_following(follower);

    return follower;
}

void
compositor_keyboard_layout_follower_destroy(
    struct compositor_keyboard_layout_follower *follower
)
{
    if (follower == NULL) {
        return;
    }

    g_clear_pointer(&follower->ipc_client, compositor_ipc_client_destroy);
    g_free(follower->rules_file_path);
    g_free(follower->layout_description);
    g_free(follower->layout_name);
    g_free(follower);
}

const char *
compositor_keyboard_layout_follower_get_layout_name(
    const struct compositor_keyboard_layout_follower *follower
)
{
    return follower->layout_name;
}
