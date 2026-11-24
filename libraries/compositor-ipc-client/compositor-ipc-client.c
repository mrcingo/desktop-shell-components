#include <gio/gunixsocketaddress.h>

#include "compositor-ipc-client.h"

struct request_awaiting_reply {
    compositor_ipc_reply_callback reply_callback;
    void                         *user_data;
};

struct compositor_ipc_client {
    const struct compositor_ipc_client_handler *handler;
    void                                       *user_data;
    struct framed_message_stream               *stream;

    GQueue requests_awaiting_reply; // in the order they were sent
};

static void
deliver_reply(
    struct request_awaiting_reply *request,
    JsonNode                      *reply_payload
)
{
    if (request->reply_callback != NULL) {
        request->reply_callback(reply_payload, request->user_data);
    }

    g_free(request);
}

static void
deliver_event(
    struct compositor_ipc_client *client,
    uint32_t                      event_type,
    JsonNode                     *event_payload
)
{
    if (event_payload == NULL) {
        return;
    }

    client->handler->handle_event(client->user_data, event_type, event_payload);
}

static void
deliver_reply_to_oldest_request(
    struct compositor_ipc_client *client,
    JsonNode                     *reply_payload
)
{
    struct request_awaiting_reply *request = g_queue_pop_head(&client->requests_awaiting_reply);

    if (request == NULL) {
        g_warning("Compositor sent a reply nobody asked for");
        return;
    }

    deliver_reply(request, reply_payload);
}

static void
dispatch_parsed_message(
    struct compositor_ipc_client *client,
    uint32_t                      message_type,
    JsonNode                     *payload
)
{
    if (compositor_ipc_message_type_is_event(message_type)) {
        deliver_event(client, message_type, payload);
        return;
    }

    deliver_reply_to_oldest_request(client, payload);
}

/* Returns the parsed document, or NULL after warning about invalid JSON. */
static JsonNode *
parse_payload_or_warn(
    JsonParser *parser,
    const char *payload,
    size_t      payload_length
)
{
    GError *error = NULL;

    if (json_parser_load_from_data(parser, payload, (gssize)payload_length, &error)) {
        return json_parser_get_root(parser);
    }

    g_warning("Compositor sent invalid JSON: %s", error->message);
    g_error_free(error);

    return NULL;
}

static void
handle_received_message(
    void       *user_data,
    uint32_t    message_type,
    const char *payload,
    size_t      payload_length
)
{
    struct compositor_ipc_client *client         = user_data;
    JsonParser                   *parser         = json_parser_new_immutable();
    JsonNode                     *parsed_payload = parse_payload_or_warn(parser, payload, payload_length);

    dispatch_parsed_message(client, message_type, parsed_payload);
    g_object_unref(parser);
}

static void
handle_stream_closed(
    void         *user_data,
    const GError *error
)
{
    struct compositor_ipc_client *client = user_data;

    if (error != NULL) {
        g_warning("Compositor IPC connection failed: %s", error->message);
    }

    struct request_awaiting_reply *request;

    while ((request = g_queue_pop_head(&client->requests_awaiting_reply))) {
        deliver_reply(request, NULL);
    }

    client->handler->handle_connection_lost(client->user_data);
}

static const struct framed_message_stream_handler stream_handler = {
    .handle_received_message = handle_received_message,
    .handle_stream_closed    = handle_stream_closed,
};

static void
answer_without_connection(
    compositor_ipc_reply_callback reply_callback,
    void                         *user_data
)
{
    if (reply_callback != NULL) {
        reply_callback(NULL, user_data);
    }
}

static void
remember_request_awaiting_reply(
    struct compositor_ipc_client *client,
    compositor_ipc_reply_callback reply_callback,
    void                         *user_data
)
{
    struct request_awaiting_reply *request = g_new(struct request_awaiting_reply, 1);

    request->reply_callback = reply_callback;
    request->user_data      = user_data;

    g_queue_push_tail(&client->requests_awaiting_reply, request);
}

void
compositor_ipc_client_send_request(
    struct compositor_ipc_client    *client,
    enum compositor_ipc_request_type request_type,
    const char                      *payload,
    compositor_ipc_reply_callback    reply_callback,
    void                            *user_data
)
{
    if (framed_message_stream_is_closed(client->stream)) {
        answer_without_connection(reply_callback, user_data);
        return;
    }

    remember_request_awaiting_reply(client, reply_callback, user_data);
    framed_message_stream_send_frame(client->stream, compositor_ipc_encode_message(request_type, payload));
}

struct compositor_ipc_client *
compositor_ipc_client_create_for_connection(
    GSocketConnection                          *connection,
    const struct compositor_ipc_client_handler *handler,
    void                                       *user_data
)
{
    struct compositor_ipc_client *client = g_new0(struct compositor_ipc_client, 1);

    client->handler   = handler;
    client->user_data = user_data;
    client->stream    = framed_message_stream_create(connection, &compositor_ipc_message_format, &stream_handler, client);

    g_queue_init(&client->requests_awaiting_reply);

    return client;
}

static GSocketConnection *
connect_to_unix_socket(
    const char *socket_path,
    GError    **error
)
{
    GSocketClient     *socket_client = g_socket_client_new();
    GSocketAddress    *address       = g_unix_socket_address_new(socket_path);
    GSocketConnection *connection    = g_socket_client_connect(socket_client, G_SOCKET_CONNECTABLE(address), NULL, error);

    g_object_unref(address);
    g_object_unref(socket_client);

    return connection;
}

struct compositor_ipc_client *
compositor_ipc_client_connect(
    const char                                 *socket_path,
    const struct compositor_ipc_client_handler *handler,
    void                                       *user_data,
    GError                                    **error
)
{
    GSocketConnection *connection = connect_to_unix_socket(socket_path, error);

    if (connection == NULL) {
        return NULL;
    }

    struct compositor_ipc_client *client = compositor_ipc_client_create_for_connection(connection, handler, user_data);

    g_object_unref(connection);

    return client;
}

void
compositor_ipc_client_destroy(
    struct compositor_ipc_client *client
)
{
    if (client == NULL) {
        return;
    }

    framed_message_stream_destroy(client->stream);
    g_queue_clear_full(&client->requests_awaiting_reply, g_free);
    g_free(client);
}
