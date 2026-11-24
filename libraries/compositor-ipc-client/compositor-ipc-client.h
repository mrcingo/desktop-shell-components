#ifndef COMPOSITOR_IPC_CLIENT_H
#define COMPOSITOR_IPC_CLIENT_H

#include <stdint.h>

#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "compositor-ipc-message-format.h"

/*
 * A client for the compositor's IPC socket: requests with replies matched in
 * order, and events once subscribed.
 */

struct compositor_ipc_client;

struct compositor_ipc_client_handler {
    void (*handle_event)(void *user_data, uint32_t event_type, JsonNode *event_payload);
    /*
     * The connection is gone and requests still waiting have received a NULL
     * reply. May destroy the client.
     */
    void (*handle_connection_lost)(void *user_data);
};

/* reply_payload is NULL if the connection broke before the reply came. */
typedef void (*compositor_ipc_reply_callback)(JsonNode *reply_payload, void *user_data);

/* socket_path is usually the value of $SWAYSOCK. */
struct compositor_ipc_client *compositor_ipc_client_connect(
    const char                                 *socket_path,
    const struct compositor_ipc_client_handler *handler,
    void                                       *user_data,
    GError                                    **error
);
/* Takes over an already connected socket, as tests do. */
struct compositor_ipc_client *compositor_ipc_client_create_for_connection(
    GSocketConnection                          *connection,
    const struct compositor_ipc_client_handler *handler,
    void                                       *user_data
);
void compositor_ipc_client_destroy(struct compositor_ipc_client *client);

/* reply_callback may be NULL to ignore the reply. */
void compositor_ipc_client_send_request(
    struct compositor_ipc_client    *client,
    enum compositor_ipc_request_type request_type,
    const char                      *payload,
    compositor_ipc_reply_callback    reply_callback,
    void                            *user_data
);

#endif
