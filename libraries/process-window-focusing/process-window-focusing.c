#include <inttypes.h>

#include <glib.h>

#include "compositor-window-tree.h"
#include "process-window-focusing.h"
#include "process-window-lookup.h"

struct focusing_request {
    struct compositor_ipc_client    *client;
    int                              process_id;
    char                            *application_name;
    process_window_focusing_callback callback;
    void                            *user_data;
};

static void
finish_request(
    struct focusing_request *request,
    bool                     focused
)
{
    process_window_focusing_callback callback  = request->callback;
    void                            *user_data = request->user_data;

    g_free(request->application_name);
    g_free(request);

    if (callback != NULL) {
        callback(focused, user_data);
    }
}

static void
focus_window(
    struct compositor_ipc_client   *client,
    const struct compositor_window *window
)
{
    g_autofree char *command = g_strdup_printf("[con_id=%" PRId64 "] focus", window->container_id);

    compositor_ipc_client_send_request(client, COMPOSITOR_IPC_REQUEST_RUN_COMMAND, command, NULL, NULL);
}

/* Returns whether a window was focused. tree may be NULL. */
static bool
focus_matching_window(
    struct focusing_request             *request,
    const struct compositor_window_tree *tree
)
{
    if (tree == NULL) {
        return false;
    }

    const struct compositor_window
        *window = process_window_lookup_find(tree, request->process_id, request->application_name, "/proc");

    if (window == NULL) {
        return false;
    }

    focus_window(request->client, window);

    return true;
}

/* NULL when the connection broke or the reply is no tree. */
static struct compositor_window_tree *
parse_tree(
    JsonNode *reply_payload
)
{
    if (reply_payload == NULL) {
        return NULL;
    }

    return compositor_window_tree_parse(reply_payload);
}

static void
handle_window_tree(
    JsonNode *reply_payload,
    void     *user_data
)
{
    struct compositor_window_tree *tree    = parse_tree(reply_payload);
    bool                           focused = focus_matching_window(user_data, tree);

    compositor_window_tree_destroy(tree);
    finish_request(user_data, focused);
}

void
process_window_focusing_focus(
    struct compositor_ipc_client    *client,
    int                              process_id,
    const char                      *application_name,
    process_window_focusing_callback callback,
    void                            *user_data
)
{
    struct focusing_request *request = g_new0(struct focusing_request, 1);

    *request = (struct focusing_request){
        .client           = client,
        .process_id       = process_id,
        .application_name = g_strdup(application_name),
        .callback         = callback,
        .user_data        = user_data,
    };

    compositor_ipc_client_send_request(client, COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE, NULL, handle_window_tree, request);
}
