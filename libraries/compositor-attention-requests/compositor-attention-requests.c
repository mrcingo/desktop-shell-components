#include <glib.h>

#include "compositor-attention-requests.h"
#include "compositor-ipc-client.h"
#include "compositor-window-tree.h"
#include "typed-json-member-lookup.h"

static const char subscribed_event_names[] = "[\"window\"]";

/* Focusing an urgent window reports "urgent" too, as the urgency ends. */
static const char *const counting_changes[] = {"urgent", "close", NULL};

struct compositor_attention_requests {
    struct compositor_ipc_client                       *ipc_client;
    const struct compositor_attention_requests_handler *handler;
    void                                               *user_data;
    int                                                 urgent_window_count;
};

static int
count_urgent_windows(
    const struct compositor_window_tree *tree
)
{
    int urgent_window_count = 0;

    for (guint i = 0; i < tree->all_windows->len; i++) {
        const struct compositor_window *window = g_ptr_array_index(tree->all_windows, i);

        urgent_window_count += window->is_urgent;
    }

    return urgent_window_count;
}

static void
report_count(
    struct compositor_attention_requests *requests,
    int                                   urgent_window_count
)
{
    if (urgent_window_count == requests->urgent_window_count) {
        return;
    }

    requests->urgent_window_count = urgent_window_count;

    requests->handler->handle_count_changed(requests->user_data, urgent_window_count);
}

static void
handle_tree_reply(
    JsonNode *reply_payload,
    void     *user_data
)
{
    struct compositor_window_tree *tree = compositor_window_tree_parse(reply_payload);

    if (tree == NULL) {
        return;
    }

    report_count(user_data, count_urgent_windows(tree));
    compositor_window_tree_destroy(tree);
}

static void
request_tree(
    struct compositor_attention_requests *requests
)
{
    compositor_ipc_client_send_request(
        requests->ipc_client,
        COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE,
        NULL,
        handle_tree_reply,
        requests
    );
}

static void
handle_event(
    void     *user_data,
    uint32_t  event_type,
    JsonNode *event_payload
)
{
    const char *change = typed_json_lookup_string_member(typed_json_root_object(event_payload), "change");

    if (event_type != COMPOSITOR_IPC_EVENT_WINDOW_CHANGED) {
        return;
    }

    if (change == NULL) {
        return;
    }

    if (g_strv_contains(counting_changes, change)) {
        request_tree(user_data);
    }
}

/* The last count stays shown; the compositor is gone or restarting. */
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
    struct compositor_attention_requests *requests
)
{
    compositor_ipc_client_send_request(
        requests->ipc_client,
        COMPOSITOR_IPC_REQUEST_SUBSCRIBE_TO_EVENTS,
        subscribed_event_names,
        NULL,
        NULL
    );
    request_tree(requests);
}

static struct compositor_attention_requests *
create_requests(
    const struct compositor_attention_requests_handler *handler,
    void                                               *user_data
)
{
    struct compositor_attention_requests *requests = g_new0(struct compositor_attention_requests, 1);

    requests->handler   = handler;
    requests->user_data = user_data;

    return requests;
}

struct compositor_attention_requests *
compositor_attention_requests_connect(
    const char                                         *socket_path,
    const struct compositor_attention_requests_handler *handler,
    void                                               *user_data,
    GError                                            **error
)
{
    struct compositor_attention_requests *requests = create_requests(handler, user_data);

    requests->ipc_client = compositor_ipc_client_connect(socket_path, &ipc_client_handler, requests, error);

    if (requests->ipc_client == NULL) {
        compositor_attention_requests_destroy(requests);
        return NULL;
    }

    start_following(requests);

    return requests;
}

struct compositor_attention_requests *
compositor_attention_requests_create_for_connection(
    GSocketConnection                                  *connection,
    const struct compositor_attention_requests_handler *handler,
    void                                               *user_data
)
{
    struct compositor_attention_requests *requests = create_requests(handler, user_data);

    requests->ipc_client = compositor_ipc_client_create_for_connection(connection, &ipc_client_handler, requests);

    start_following(requests);

    return requests;
}

void
compositor_attention_requests_destroy(
    struct compositor_attention_requests *requests
)
{
    if (requests == NULL) {
        return;
    }

    g_clear_pointer(&requests->ipc_client, compositor_ipc_client_destroy);
    g_free(requests);
}

int
compositor_attention_requests_get_count(
    const struct compositor_attention_requests *requests
)
{
    return requests->urgent_window_count;
}

void
compositor_attention_requests_focus_latest(
    struct compositor_attention_requests *requests
)
{
    compositor_ipc_client_send_request(
        requests->ipc_client,
        COMPOSITOR_IPC_REQUEST_RUN_COMMAND,
        "[urgent=latest] focus",
        NULL,
        NULL
    );
}
