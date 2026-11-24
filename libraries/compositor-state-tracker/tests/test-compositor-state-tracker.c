#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "compositor-ipc-message-format.h"
#include "compositor-state-tracker.h"

static const char window_tree_with_two_windows[] = "{\"id\": 1, \"type\": \"root\", \"focus\": [2], \"nodes\": [{"
                                                   "\"id\": 2, \"type\": \"output\", \"name\": \"eDP-1\", \"focus\": [3],"
                                                   "\"nodes\": [{\"id\": 3, \"type\": \"workspace\", \"focus\": [10, 11],"
                                                   "\"nodes\": ["
                                                   "{\"id\": 10, \"pid\": 100, \"app_id\": \"one\", \"focused\": true},"
                                                   "{\"id\": 11, \"pid\": 101, \"app_id\": \"two\"}"
                                                   "]}]}]}";

struct tracker_test_fixture {
    int                              compositor_socket;
    struct compositor_state_tracker *tracker;
    int                              tree_update_count;
    char                            *last_binding_command;
    bool                             compositor_gone;
};

static void
count_tree_update(
    void                                *user_data,
    const struct compositor_window_tree *tree
)
{
    struct tracker_test_fixture *fixture = user_data;

    fixture->tree_update_count++;
}

static void
record_binding_command(
    void       *user_data,
    const char *command
)
{
    struct tracker_test_fixture *fixture = user_data;

    g_free(fixture->last_binding_command);

    fixture->last_binding_command = g_strdup(command);
}

static void
record_compositor_gone(
    void *user_data
)
{
    struct tracker_test_fixture *fixture = user_data;

    fixture->compositor_gone = true;
}

static const struct compositor_state_tracker_handler recording_handler = {
    .handle_window_tree_updated = count_tree_update,
    .handle_key_binding_command = record_binding_command,
    .handle_compositor_gone     = record_compositor_gone,
};

static void
compositor_sends(
    struct tracker_test_fixture *fixture,
    uint32_t                     message_type,
    const char                  *payload
)
{
    GBytes     *message = compositor_ipc_encode_message(message_type, payload);
    size_t      size;
    const void *bytes = g_bytes_get_data(message, &size);

    g_assert_cmpint(write(fixture->compositor_socket, bytes, size), ==, (ssize_t)size);
    g_bytes_unref(message);
}

static void
iterate_main_context_briefly(void)
{
    for (int i = 0; i < 20; i++) {
        g_main_context_iteration(NULL, FALSE);
    }
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

static uint32_t
receive_message_header(
    struct tracker_test_fixture *fixture,
    uint32_t                    *payload_length
)
{
    uint8_t  header[COMPOSITOR_IPC_HEADER_SIZE];
    uint32_t message_type = 0;

    iterate_main_context_briefly();
    read_exactly(fixture->compositor_socket, header, sizeof(header));
    g_assert_true(compositor_ipc_message_format.decode_header(header, payload_length, &message_type));

    return message_type;
}

static void
discard_payload(
    struct tracker_test_fixture *fixture,
    uint32_t                     payload_length
)
{
    char *payload = g_malloc0(payload_length + 1);

    read_exactly(fixture->compositor_socket, payload, payload_length);
    g_free(payload);
}

static uint32_t
compositor_receives_request(
    struct tracker_test_fixture *fixture
)
{
    uint32_t payload_length = 0;
    uint32_t message_type   = receive_message_header(fixture, &payload_length);

    discard_payload(fixture, payload_length);

    return message_type;
}

static void
iterate_until_tree_update_count(
    struct tracker_test_fixture *fixture,
    int                          count
)
{
    for (int i = 0; i < 1000 && fixture->tree_update_count < count; i++) {
        g_main_context_iteration(NULL, TRUE);
    }

    g_assert_cmpint(fixture->tree_update_count, ==, count);
}

static void
iterate_until_compositor_gone(
    struct tracker_test_fixture *fixture
)
{
    for (int i = 0; i < 1000 && !fixture->compositor_gone; i++) {
        g_main_context_iteration(NULL, TRUE);
    }
}

static void
compositor_answers_tree_request(
    struct tracker_test_fixture *fixture,
    int                          expected_update_count
)
{
    compositor_sends(fixture, COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE, window_tree_with_two_windows);
    iterate_until_tree_update_count(fixture, expected_update_count);
}

static GSocketConnection *
wrap_socket(
    int file_descriptor
)
{
    GSocket           *socket     = g_socket_new_from_fd(file_descriptor, NULL);
    GSocketConnection *connection = g_socket_connection_factory_create_connection(socket);

    g_object_unref(socket);

    return connection;
}

static GSocketConnection *
create_connection_from_socket_pair(
    int *compositor_socket
)
{
    int sockets[2];

    g_assert_cmpint(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), ==, 0);

    *compositor_socket = sockets[1];

    return wrap_socket(sockets[0]);
}

static void
connect_tracker_to_socket_pair(
    struct tracker_test_fixture *fixture
)
{
    GSocketConnection *connection = create_connection_from_socket_pair(&fixture->compositor_socket);

    fixture->tracker = compositor_state_tracker_create_for_connection(connection, &recording_handler, fixture);

    g_object_unref(connection);
}

static void
answer_initial_requests(
    struct tracker_test_fixture *fixture
)
{
    g_assert_cmpuint(compositor_receives_request(fixture), ==, COMPOSITOR_IPC_REQUEST_SUBSCRIBE_TO_EVENTS);
    g_assert_cmpuint(compositor_receives_request(fixture), ==, COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE);
    compositor_sends(fixture, COMPOSITOR_IPC_REQUEST_SUBSCRIBE_TO_EVENTS, "{\"success\": true}");
    compositor_answers_tree_request(fixture, 1);
}

static void
set_up_fixture(
    struct tracker_test_fixture *fixture
)
{
    connect_tracker_to_socket_pair(fixture);
    answer_initial_requests(fixture);
}

static void
tear_down_fixture(
    struct tracker_test_fixture *fixture
)
{
    compositor_state_tracker_destroy(fixture->tracker);
    g_free(fixture->last_binding_command);
    close(fixture->compositor_socket);
}

static void
assert_focus_position(
    struct tracker_test_fixture *fixture,
    int64_t                      window_id,
    int                          expected_position
)
{
    const struct window_focus_history *history = compositor_state_tracker_get_focus_history(fixture->tracker);

    g_assert_cmpint(window_focus_history_position_of(history, window_id), ==, expected_position);
}

static void
compositor_reports_focus_of_window_eleven(
    struct tracker_test_fixture *fixture
)
{
    compositor_sends(fixture, COMPOSITOR_IPC_EVENT_WINDOW_CHANGED, "{\"change\": \"focus\", \"container\": {\"id\": 11}}");
    g_assert_cmpuint(compositor_receives_request(fixture), ==, COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE);
}

static void
compositor_reports_binding_then_shutdown(
    struct tracker_test_fixture *fixture
)
{
    compositor_sends(
        fixture,
        COMPOSITOR_IPC_EVENT_KEY_BINDING_TRIGGERED,
        "{\"change\": \"run\", \"binding\": {\"command\": \"nop window-switcher next\"}}"
    );
    compositor_sends(fixture, COMPOSITOR_IPC_EVENT_COMPOSITOR_SHUTTING_DOWN, "{\"change\": \"exit\"}");
}

static void
test_first_tree_seeds_focus_history(void)
{
    struct tracker_test_fixture fixture = {0};

    set_up_fixture(&fixture);
    g_assert_nonnull(compositor_state_tracker_get_window_tree(fixture.tracker));
    assert_focus_position(&fixture, 10, 0);
    assert_focus_position(&fixture, 11, 1);
    tear_down_fixture(&fixture);
}

/* The tree's own focus order does not undo the recorded focus. */
static void
test_focus_event_updates_history_and_tree(void)
{
    struct tracker_test_fixture fixture = {0};

    set_up_fixture(&fixture);
    compositor_reports_focus_of_window_eleven(&fixture);
    assert_focus_position(&fixture, 11, 0);
    compositor_answers_tree_request(&fixture, 2);
    assert_focus_position(&fixture, 11, 0);
    tear_down_fixture(&fixture);
}

static void
test_binding_commands_and_shutdown_are_reported(void)
{
    struct tracker_test_fixture fixture = {0};

    set_up_fixture(&fixture);
    compositor_reports_binding_then_shutdown(&fixture);
    iterate_until_compositor_gone(&fixture);
    g_assert_cmpstr(fixture.last_binding_command, ==, "nop window-switcher next");
    g_assert_true(fixture.compositor_gone);
    tear_down_fixture(&fixture);
}

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/compositor-state-tracker/first-tree", test_first_tree_seeds_focus_history);
    g_test_add_func("/compositor-state-tracker/focus-event", test_focus_event_updates_history_and_tree);
    g_test_add_func("/compositor-state-tracker/binding-and-shutdown", test_binding_commands_and_shutdown_are_reported);

    return g_test_run();
}
