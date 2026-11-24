#include <inttypes.h>
#include <string.h>

#include "compositor-ipc-client.h"
#include "compositor-state-tracker.h"
#include "typed-json-member-lookup.h"

static const char subscribed_event_names[] = "[\"window\", \"workspace\", \"output\", \"binding\", \"shutdown\"]";

struct compositor_state_tracker {
    const struct compositor_state_tracker_handler *handler;
    void                                          *user_data;
    bool                                           follows_title_changes;
    struct compositor_ipc_client                  *ipc_client;

    struct compositor_window_tree *window_tree; // NULL until the first reply
    struct window_focus_history   *focus_history;

    /* A burst of events costs one tree request in flight plus one after. */
    bool tree_request_in_flight;
    bool tree_changed_while_in_flight;
};

static void request_window_tree(struct compositor_state_tracker *tracker);

/* Returns NULL when the connection broke or the tree is malformed. */
static struct compositor_window_tree *
parse_window_tree_reply(
    JsonNode *reply_payload
)
{
    if (reply_payload == NULL) {
        return NULL; // the connection is gone and the handler was told
    }

    struct compositor_window_tree *tree = compositor_window_tree_parse(reply_payload);

    if (tree == NULL) {
        g_warning("Compositor sent a malformed window tree");
    }

    return tree;
}

static void
replace_window_tree(
    struct compositor_state_tracker *tracker,
    struct compositor_window_tree   *tree
)
{
    struct compositor_window_tree *previous_tree = tracker->window_tree;

    tracker->window_tree = tree;

    compositor_window_tree_destroy(previous_tree);
    window_focus_history_synchronize(tracker->focus_history, tree->window_ids_in_focus_order);
}

static void
request_window_tree_again_if_changed(
    struct compositor_state_tracker *tracker
)
{
    if (!tracker->tree_changed_while_in_flight) {
        return;
    }

    tracker->tree_changed_while_in_flight = false;

    request_window_tree(tracker);
}

static void
adopt_window_tree(
    struct compositor_state_tracker *tracker,
    struct compositor_window_tree   *tree
)
{
    replace_window_tree(tracker, tree);
    request_window_tree_again_if_changed(tracker);
}

static void
handle_window_tree_reply(
    JsonNode *reply_payload,
    void     *user_data
)
{
    struct compositor_state_tracker *tracker = user_data;
    struct compositor_window_tree   *tree    = parse_window_tree_reply(reply_payload);

    tracker->tree_request_in_flight = false;

    if (tree == NULL) {
        return;
    }

    adopt_window_tree(tracker, tree);
    tracker->handler->handle_window_tree_updated(tracker->user_data, tree);
}

static void
request_window_tree(
    struct compositor_state_tracker *tracker
)
{
    if (tracker->tree_request_in_flight) {
        tracker->tree_changed_while_in_flight = true;
        return;
    }

    tracker->tree_request_in_flight = true;

    compositor_ipc_client_send_request(
        tracker->ipc_client,
        COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE,
        NULL,
        handle_window_tree_reply,
        tracker
    );
}

static int64_t
event_container_id(
    JsonObject *event
)
{
    JsonObject *container    = typed_json_lookup_object_member(event, "container");
    int64_t     container_id = -1;

    typed_json_lookup_integer_member(container, "id", &container_id);

    return container_id;
}

/* Changes that alter which windows a workspace holds, their order or their focus. */
static const char *const window_list_changes[] = {"new", "close", "focus", "move", "floating"};

static bool
is_window_list_change(
    const char *change
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(window_list_changes); i++) {
        if (g_strcmp0(change, window_list_changes[i]) == 0) {
            return true;
        }
    }

    return false;
}

static bool
title_change_matters(
    struct compositor_state_tracker *tracker,
    JsonObject                      *event
)
{
    if (tracker->follows_title_changes) {
        return true;
    }

    return compositor_window_name_comes_from_title(typed_json_lookup_object_member(event, "container"));
}

/*
 * Title, mark, urgency and fullscreen changes are frequent (a terminal
 * retitles on every command) and change nothing the components show,
 * except the name of a window that is named after its title.
 */
static bool
window_event_changes_tree(
    struct compositor_state_tracker *tracker,
    JsonObject                      *event
)
{
    const char *change = typed_json_lookup_string_member(event, "change");

    if (is_window_list_change(change)) {
        return true;
    }

    if (g_strcmp0(change, "title") != 0) {
        return false;
    }

    return title_change_matters(tracker, event);
}

static void
update_focus_history(
    struct compositor_state_tracker *tracker,
    JsonObject                      *event
)
{
    const char *change = typed_json_lookup_string_member(event, "change");

    if (g_strcmp0(change, "focus") == 0) {
        window_focus_history_record_focus(tracker->focus_history, event_container_id(event));
    }

    if (g_strcmp0(change, "close") == 0) {
        window_focus_history_forget_window(tracker->focus_history, event_container_id(event));
    }
}

static void
handle_window_event(
    struct compositor_state_tracker *tracker,
    JsonObject                      *event
)
{
    update_focus_history(tracker, event);

    if (window_event_changes_tree(tracker, event)) {
        request_window_tree(tracker);
    }
}

static void
handle_binding_event(
    struct compositor_state_tracker *tracker,
    JsonObject                      *event
)
{
    JsonObject *binding = typed_json_lookup_object_member(event, "binding");
    const char *command = typed_json_lookup_string_member(binding, "command");

    if (command == NULL) {
        return;
    }

    tracker->handler->handle_key_binding_command(tracker->user_data, command);
}

static void
handle_layout_event(
    struct compositor_state_tracker *tracker,
    JsonObject                      *event
)
{
    request_window_tree(tracker);
}

static void
report_configuration_reload(
    struct compositor_state_tracker *tracker
)
{
    if (tracker->handler->handle_compositor_configuration_reloaded == NULL) {
        return;
    }

    tracker->handler->handle_compositor_configuration_reloaded(tracker->user_data);
}

/* sway reports a configuration reload as a workspace event with change "reload". */
static void
handle_workspace_event(
    struct compositor_state_tracker *tracker,
    JsonObject                      *event
)
{
    const char *change = typed_json_lookup_string_member(event, "change");

    if (g_strcmp0(change, "reload") == 0) {
        report_configuration_reload(tracker);
    }

    request_window_tree(tracker);
}

static void
handle_shutdown_event(
    struct compositor_state_tracker *tracker,
    JsonObject                      *event
)
{
    tracker->handler->handle_compositor_gone(tracker->user_data);
}

static const struct {
    uint32_t event_type;

    void (*handle)(struct compositor_state_tracker *tracker, JsonObject *event);
} event_handlers[] = {
    {COMPOSITOR_IPC_EVENT_WINDOW_CHANGED,           handle_window_event   },
    {COMPOSITOR_IPC_EVENT_WORKSPACE_CHANGED,        handle_workspace_event},
    {COMPOSITOR_IPC_EVENT_OUTPUT_CHANGED,           handle_layout_event   },
    {COMPOSITOR_IPC_EVENT_KEY_BINDING_TRIGGERED,    handle_binding_event  },
    {COMPOSITOR_IPC_EVENT_COMPOSITOR_SHUTTING_DOWN, handle_shutdown_event },
};

static void
dispatch_event(
    struct compositor_state_tracker *tracker,
    uint32_t                         event_type,
    JsonObject                      *event
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(event_handlers); i++) {
        if (event_handlers[i].event_type == event_type) {
            event_handlers[i].handle(tracker, event);
        }
    }
}

static void
handle_compositor_event(
    void     *user_data,
    uint32_t  event_type,
    JsonNode *event_payload
)
{
    struct compositor_state_tracker *tracker = user_data;
    JsonObject                      *event   = typed_json_root_object(event_payload);

    if (event == NULL) {
        return;
    }

    dispatch_event(tracker, event_type, event);
}

static void
handle_connection_lost(
    void *user_data
)
{
    struct compositor_state_tracker *tracker = user_data;

    tracker->handler->handle_compositor_gone(tracker->user_data);
}

static const struct compositor_ipc_client_handler ipc_client_handler = {
    .handle_event           = handle_compositor_event,
    .handle_connection_lost = handle_connection_lost,
};

static void
handle_subscription_reply(
    JsonNode *reply_payload,
    void     *user_data
)
{
    JsonObject *reply = typed_json_root_object(reply_payload);

    if (reply == NULL) {
        return;
    }

    if (!typed_json_lookup_boolean_member(reply, "success", false)) {
        g_warning("Compositor refused the event subscription");
    }
}

static struct compositor_state_tracker *
create_tracker(
    const struct compositor_state_tracker_handler *handler,
    void                                          *user_data
)
{
    struct compositor_state_tracker *tracker = g_new0(struct compositor_state_tracker, 1);

    tracker->handler       = handler;
    tracker->user_data     = user_data;
    tracker->focus_history = window_focus_history_create();

    return tracker;
}

/* Subscribing first means no change between the two replies is missed. */
static void
start_tracking(
    struct compositor_state_tracker *tracker
)
{
    compositor_ipc_client_send_request(
        tracker->ipc_client,
        COMPOSITOR_IPC_REQUEST_SUBSCRIBE_TO_EVENTS,
        subscribed_event_names,
        handle_subscription_reply,
        tracker
    );
    request_window_tree(tracker);
}

struct compositor_state_tracker *
compositor_state_tracker_create_for_connection(
    GSocketConnection                             *connection,
    const struct compositor_state_tracker_handler *handler,
    void                                          *user_data
)
{
    struct compositor_state_tracker *tracker = create_tracker(handler, user_data);

    tracker->ipc_client = compositor_ipc_client_create_for_connection(connection, &ipc_client_handler, tracker);

    start_tracking(tracker);

    return tracker;
}

struct compositor_state_tracker *
compositor_state_tracker_connect(
    const char                                    *socket_path,
    const struct compositor_state_tracker_handler *handler,
    void                                          *user_data,
    GError                                       **error
)
{
    struct compositor_state_tracker *tracker = create_tracker(handler, user_data);

    tracker->ipc_client = compositor_ipc_client_connect(socket_path, &ipc_client_handler, tracker, error);

    if (tracker->ipc_client == NULL) {
        compositor_state_tracker_destroy(tracker);
        return NULL;
    }

    start_tracking(tracker);

    return tracker;
}

void
compositor_state_tracker_destroy(
    struct compositor_state_tracker *tracker
)
{
    if (tracker == NULL) {
        return;
    }

    compositor_ipc_client_destroy(tracker->ipc_client);
    compositor_window_tree_destroy(tracker->window_tree);
    window_focus_history_destroy(tracker->focus_history);
    g_free(tracker);
}

const struct compositor_window_tree *
compositor_state_tracker_get_window_tree(
    struct compositor_state_tracker *tracker
)
{
    return tracker->window_tree;
}

const struct window_focus_history *
compositor_state_tracker_get_focus_history(
    struct compositor_state_tracker *tracker
)
{
    return tracker->focus_history;
}

static void
run_command_on_window(
    struct compositor_state_tracker *tracker,
    int64_t                          container_id,
    const char                      *command
)
{
    char *addressed_command = g_strdup_printf("[con_id=%" PRId64 "] %s", container_id, command);

    compositor_ipc_client_send_request(tracker->ipc_client, COMPOSITOR_IPC_REQUEST_RUN_COMMAND, addressed_command, NULL, NULL);
    g_free(addressed_command);
}

void
compositor_state_tracker_focus_window(
    struct compositor_state_tracker *tracker,
    int64_t                          container_id
)
{
    run_command_on_window(tracker, container_id, "focus");
}

void
compositor_state_tracker_close_window(
    struct compositor_state_tracker *tracker,
    int64_t                          container_id
)
{
    run_command_on_window(tracker, container_id, "kill");
}

void
compositor_state_tracker_follow_title_changes(
    struct compositor_state_tracker *tracker,
    bool                             follows_title_changes
)
{
    tracker->follows_title_changes = follows_title_changes;
}

struct compositor_ipc_client *
compositor_state_tracker_get_ipc_client(
    struct compositor_state_tracker *tracker
)
{
    return tracker->ipc_client;
}

void
compositor_state_tracker_refresh_window_tree(
    struct compositor_state_tracker *tracker
)
{
    request_window_tree(tracker);
}
