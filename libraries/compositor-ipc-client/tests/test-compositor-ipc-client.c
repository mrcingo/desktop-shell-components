#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "compositor-ipc-client.h"

struct client_test_fixture {
    int                           compositor_socket;
    struct compositor_ipc_client *client;

    GPtrArray *log; // char *, what arrived in order
    bool       connection_lost;
};

static void
record_event(
    void     *user_data,
    uint32_t  event_type,
    JsonNode *event_payload
)
{
    struct client_test_fixture *fixture = user_data;

    g_ptr_array_add(fixture->log, g_strdup_printf("event %x", event_type));
}

static void
record_connection_lost(
    void *user_data
)
{
    struct client_test_fixture *fixture = user_data;

    fixture->connection_lost = true;
}

static const struct compositor_ipc_client_handler recording_handler = {
    .handle_event           = record_event,
    .handle_connection_lost = record_connection_lost,
};

static void
record_reply(
    JsonNode *reply_payload,
    void     *user_data
)
{
    struct client_test_fixture *fixture = user_data;

    if (reply_payload == NULL) {
        g_ptr_array_add(fixture->log, g_strdup("reply none"));
        return;
    }

    JsonObject *object = json_node_get_object(reply_payload);

    g_ptr_array_add(fixture->log, g_strdup_printf("reply %s", json_object_get_string_member(object, "name")));
}

static int
create_connected_socket_pair(
    int *compositor_socket
)
{
    int sockets[2];

    g_assert_cmpint(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), ==, 0);

    *compositor_socket = sockets[1];

    return sockets[0];
}

static struct compositor_ipc_client *
create_client_for_socket(
    int                         socket_descriptor,
    struct client_test_fixture *fixture
)
{
    GSocket                      *socket     = g_socket_new_from_fd(socket_descriptor, NULL);
    GSocketConnection            *connection = g_socket_connection_factory_create_connection(socket);
    struct compositor_ipc_client *client = compositor_ipc_client_create_for_connection(connection, &recording_handler, fixture);

    g_object_unref(connection);
    g_object_unref(socket);

    return client;
}

static void
set_up_fixture(
    struct client_test_fixture *fixture
)
{
    int client_socket = create_connected_socket_pair(&fixture->compositor_socket);

    fixture->log    = g_ptr_array_new_with_free_func(g_free);
    fixture->client = create_client_for_socket(client_socket, fixture);
}

static void
tear_down_fixture(
    struct client_test_fixture *fixture
)
{
    compositor_ipc_client_destroy(fixture->client);
    g_ptr_array_unref(fixture->log);
    close(fixture->compositor_socket);
}

static void
compositor_sends(
    struct client_test_fixture *fixture,
    uint32_t                    message_type,
    const char                 *payload
)
{
    GBytes     *message = compositor_ipc_encode_message(message_type, payload);
    size_t      size;
    const void *bytes = g_bytes_get_data(message, &size);

    g_assert_cmpint(write(fixture->compositor_socket, bytes, size), ==, (ssize_t)size);
    g_bytes_unref(message);
}

static uint32_t
compositor_reads_header(
    struct client_test_fixture *fixture,
    uint32_t                   *payload_length
)
{
    uint8_t  header[COMPOSITOR_IPC_HEADER_SIZE];
    uint32_t message_type;

    g_assert_cmpint(read(fixture->compositor_socket, header, sizeof(header)), ==, sizeof(header));
    g_assert_true(compositor_ipc_message_format.decode_header(header, payload_length, &message_type));

    return message_type;
}

static char *
compositor_reads_payload(
    struct client_test_fixture *fixture,
    uint32_t                    payload_length
)
{
    char *payload = g_malloc0(payload_length + 1);

    g_assert_cmpint(read(fixture->compositor_socket, payload, payload_length), ==, (ssize_t)payload_length);

    return payload;
}

static void
compositor_receives(
    struct client_test_fixture *fixture,
    uint32_t                    expected_type,
    const char                 *expected_payload
)
{
    uint32_t payload_length;
    uint32_t message_type = compositor_reads_header(fixture, &payload_length);
    char    *payload      = compositor_reads_payload(fixture, payload_length);

    g_assert_cmpuint(message_type, ==, expected_type);
    g_assert_cmpstr(payload, ==, expected_payload);
    g_free(payload);
}

static void
iterate_until_log_length(
    struct client_test_fixture *fixture,
    guint                       length
)
{
    for (int i = 0; i < 1000 && fixture->log->len < length; i++) {
        g_main_context_iteration(NULL, TRUE);
    }

    g_assert_cmpuint(fixture->log->len, ==, length);
}

static void
send_window_tree_request(
    struct client_test_fixture *fixture
)
{
    compositor_ipc_client_send_request(fixture->client, COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE, NULL, record_reply, fixture);
}

static void
exchange_two_requests_with_compositor(
    struct client_test_fixture *fixture
)
{
    send_window_tree_request(fixture);
    compositor_ipc_client_send_request(fixture->client, COMPOSITOR_IPC_REQUEST_RUN_COMMAND, "nop", record_reply, fixture);

    for (int i = 0; i < 10; i++) {
        g_main_context_iteration(NULL, FALSE);
    }

    compositor_receives(fixture, COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE, "");
    compositor_receives(fixture, COMPOSITOR_IPC_REQUEST_RUN_COMMAND, "nop");
}

/* An event may arrive between two replies. */
static void
compositor_sends_reply_event_reply(
    struct client_test_fixture *fixture
)
{
    compositor_sends(fixture, COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE, "{\"name\": \"tree\"}");
    compositor_sends(fixture, COMPOSITOR_IPC_EVENT_WINDOW_CHANGED, "{\"change\": \"new\"}");
    compositor_sends(fixture, COMPOSITOR_IPC_REQUEST_RUN_COMMAND, "{\"name\": \"command\"}");
}

static void
assert_log_matches(
    struct client_test_fixture *fixture,
    const char *const          *expected_entries
)
{
    for (guint i = 0; expected_entries[i] != NULL; i++) {
        g_assert_cmpstr(g_ptr_array_index(fixture->log, i), ==, expected_entries[i]);
    }
}

static const char *const replies_around_event[] = {"reply tree", "event 80000003", "reply command", NULL};

static void
test_replies_match_requests_between_events(void)
{
    struct client_test_fixture fixture = {0};

    set_up_fixture(&fixture);
    exchange_two_requests_with_compositor(&fixture);
    compositor_sends_reply_event_reply(&fixture);
    iterate_until_log_length(&fixture, 3);
    assert_log_matches(&fixture, replies_around_event);
    tear_down_fixture(&fixture);
}

static void
send_request_then_hang_up(
    struct client_test_fixture *fixture
)
{
    send_window_tree_request(fixture);
    shutdown(fixture->compositor_socket, SHUT_WR);
}

static void
assert_waiting_request_got_no_reply(
    struct client_test_fixture *fixture
)
{
    g_assert_cmpstr(g_ptr_array_index(fixture->log, 0), ==, "reply none");
    g_assert_true(fixture->connection_lost);
}

/* Requests after the connection is gone are answered at once. */
static void
assert_later_request_is_answered_at_once(
    struct client_test_fixture *fixture
)
{
    send_window_tree_request(fixture);
    g_assert_cmpuint(fixture->log->len, ==, 2);
}

static void
test_lost_connection_answers_waiting_requests(void)
{
    struct client_test_fixture fixture = {0};

    set_up_fixture(&fixture);
    send_request_then_hang_up(&fixture);
    iterate_until_log_length(&fixture, 1);
    assert_waiting_request_got_no_reply(&fixture);
    assert_later_request_is_answered_at_once(&fixture);
    tear_down_fixture(&fixture);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/compositor-ipc-client/replies-between-events", test_replies_match_requests_between_events   },
    {"/compositor-ipc-client/lost-connection",        test_lost_connection_answers_waiting_requests},
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
