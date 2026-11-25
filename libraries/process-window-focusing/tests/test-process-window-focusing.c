#include <sys/socket.h>
#include <unistd.h>

#include "compositor-ipc-message-format.h"
#include "process-window-focusing.h"

/* One window, named after Slack, on one output. */
static const char
    window_tree[] = "{\"id\": 1, \"type\": \"root\", \"focus\": [2], \"floating_nodes\": [], \"nodes\": ["
                    " {\"id\": 2, \"type\": \"output\", \"name\": \"eDP-1\", \"focus\": [3], \"floating_nodes\": [],"
                    "  \"rect\": {\"x\": 0, \"y\": 0, \"width\": 1920, \"height\": 1080}, \"nodes\": ["
                    "  {\"id\": 3, \"type\": \"workspace\", \"name\": \"1\", \"focus\": [11], \"floating_nodes\": [],"
                    "   \"rect\": {\"x\": 0, \"y\": 0, \"width\": 1920, \"height\": 1080}, \"nodes\": ["
                    "   {\"id\": 11, \"type\": \"con\", \"name\": \"Slack | general\", \"app_id\": \"Slack\","
                    "    \"pid\": 4242, \"focus\": [], \"nodes\": [], \"floating_nodes\": []}]}]}]}";

struct fake_compositor {
    int                           compositor_socket;
    struct compositor_ipc_client *client;
    bool                          is_done;
    bool                          focused;
};

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

static const struct compositor_ipc_client_handler client_handler = {
    .handle_event           = ignore_event,
    .handle_connection_lost = ignore_connection_lost,
};

static GSocketConnection *
wrap_socket(
    int file_descriptor
)
{
    g_autoptr(GSocket) socket = g_socket_new_from_fd(file_descriptor, NULL);

    return g_socket_connection_factory_create_connection(socket);
}

static void
start_fake_compositor(
    struct fake_compositor *compositor
)
{
    int sockets[2];

    g_assert_cmpint(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), ==, 0);

    g_autoptr(GSocketConnection) connection = wrap_socket(sockets[0]);

    compositor->compositor_socket = sockets[1];
    compositor->client            = compositor_ipc_client_create_for_connection(connection, &client_handler, compositor);
}

static void
read_exactly(
    int    file_descriptor,
    void  *buffer,
    size_t length
)
{
    g_assert_cmpint(read(file_descriptor, buffer, length), ==, (ssize_t)length);
}

static void
let_client_send(void)
{
    for (int i = 0; i < 50; i++) {
        g_main_context_iteration(NULL, FALSE);
    }
}

static char *
read_payload(
    int      file_descriptor,
    uint32_t payload_length
)
{
    char *payload = g_malloc0(payload_length + 1);

    read_exactly(file_descriptor, payload, payload_length);

    return payload;
}

/* Reads one request and returns its payload. */
static char *
receive_request(
    struct fake_compositor *compositor,
    uint32_t               *message_type
)
{
    uint8_t  header[COMPOSITOR_IPC_HEADER_SIZE];
    uint32_t payload_length = 0;

    let_client_send();
    read_exactly(compositor->compositor_socket, header, sizeof(header));
    g_assert_true(compositor_ipc_message_format.decode_header(header, &payload_length, message_type));

    return read_payload(compositor->compositor_socket, payload_length);
}

static void
send_reply(
    struct fake_compositor *compositor,
    uint32_t                message_type,
    const char             *payload
)
{
    g_autoptr(GBytes) message = compositor_ipc_encode_message(message_type, payload);

    gsize       size  = 0;
    const void *bytes = g_bytes_get_data(message, &size);

    g_assert_cmpint(write(compositor->compositor_socket, bytes, size), ==, (ssize_t)size);
}

static void
record_result(
    bool  focused,
    void *user_data
)
{
    struct fake_compositor *compositor = user_data;

    compositor->is_done = true;
    compositor->focused = focused;
}

static void
answer_tree_request(
    struct fake_compositor *compositor
)
{
    uint32_t message_type = 0;

    g_autofree char *payload = receive_request(compositor, &message_type);

    g_assert_cmpuint(message_type, ==, COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE);
    send_reply(compositor, COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE, window_tree);

    while (!compositor->is_done) {
        g_main_context_iteration(NULL, TRUE);
    }
}

static char *
answer_tree_then_receive_command(
    struct fake_compositor *compositor,
    uint32_t               *message_type
)
{
    answer_tree_request(compositor);

    return receive_request(compositor, message_type);
}

static void
stop_fake_compositor(
    struct fake_compositor *compositor
)
{
    compositor_ipc_client_destroy(compositor->client);
    close(compositor->compositor_socket);
}

/* No process matches, so the application name finds the window. */
static void
test_window_is_focused_by_name(void)
{
    struct fake_compositor compositor   = {0};
    uint32_t               message_type = 0;

    start_fake_compositor(&compositor);
    process_window_focusing_focus(compositor.client, 0, "slack", record_result, &compositor);

    g_autofree char *command = answer_tree_then_receive_command(&compositor, &message_type);

    g_assert_true(compositor.focused && g_str_equal(command, "[con_id=11] focus"));
    stop_fake_compositor(&compositor);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/process-window-focusing/by-name", test_window_is_focused_by_name},
};

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);

    for (size_t i = 0; i < G_N_ELEMENTS(registered_tests); i++) {
        g_test_add_func(registered_tests[i].test_path, registered_tests[i].test_function);
    }

    return g_test_run();
}
