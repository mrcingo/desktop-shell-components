#include <glib.h>

#include "login-greeter-keyboard-layout.h"
#include "system-keyboard-layout.h"
#include "typed-json-member-lookup.h"

static void
ignore_event(
    void     *user_data,
    uint32_t  event_type,
    JsonNode *event_payload
)
{}

static void
ignore_connection_lost(
    void *user_data
)
{}

static const struct compositor_ipc_client_handler quiet_handler = {
    .handle_event           = ignore_event,
    .handle_connection_lost = ignore_connection_lost,
};

static JsonObject *
first_result(
    JsonArray *results
)
{
    if (typed_json_count_array_elements(results) == 0) {
        return NULL;
    }

    return typed_json_root_object(json_array_get_element(results, 0));
}

/* sway answers a command with a list of results, one per command run. */
static void
report_command_failure(
    JsonNode *reply_payload,
    void     *user_data
)
{
    if (reply_payload == NULL) {
        return;
    }

    if (!JSON_NODE_HOLDS_ARRAY(reply_payload)) {
        return;
    }

    JsonObject *result = first_result(json_node_get_array(reply_payload));

    if (!typed_json_lookup_boolean_member(result, "success", true)) {
        g_warning("sway refused the system keyboard layout: %s", typed_json_lookup_string_member(result, "error"));
    }
}

static char *
system_layout_command(void)
{
    struct system_keyboard_layout *layout = system_keyboard_layout_read();

    if (layout == NULL) {
        return NULL;
    }

    char *command = system_keyboard_layout_to_compositor_command(layout);

    system_keyboard_layout_destroy(layout);

    return command;
}

static struct compositor_ipc_client *
connect_to_own_compositor(void)
{
    const char *socket_path = g_getenv("SWAYSOCK");

    if (socket_path == NULL) {
        return NULL;
    }

    return compositor_ipc_client_connect(socket_path, &quiet_handler, NULL, NULL);
}

static struct compositor_ipc_client *
send_command_to_own_compositor(
    const char *command
)
{
    struct compositor_ipc_client *client = connect_to_own_compositor();

    if (client == NULL) {
        return NULL;
    }

    compositor_ipc_client_send_request(client, COMPOSITOR_IPC_REQUEST_RUN_COMMAND, command, report_command_failure, NULL);

    return client;
}

struct compositor_ipc_client *
login_greeter_apply_system_keyboard_layout(void)
{
    g_autofree char *command = system_layout_command();

    if (command == NULL) {
        return NULL;
    }

    return send_command_to_own_compositor(command);
}
