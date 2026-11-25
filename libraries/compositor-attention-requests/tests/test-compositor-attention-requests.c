#include <sys/socket.h>
#include <unistd.h>

#include <gio/gio.h>

#include "compositor-attention-requests.h"
#include "compositor-ipc-message-format.h"

static const char tree_with_urgent_window
    [] = "{\"id\": 1, \"type\": \"root\", \"nodes\": [{\"id\": 2, \"type\": \"output\", \"name\": \"eDP-1\","
         "\"nodes\": [{\"id\": 3, \"type\": \"workspace\", \"name\": \"1\", \"nodes\": ["
         "{\"id\": 10, \"pid\": 100, \"app_id\": \"one\", \"urgent\": true},"
         "{\"id\": 11, \"pid\": 101, \"app_id\": \"two\", \"urgent\": false}]}]}]}";

static const char tree_without_urgent_window
    [] = "{\"id\": 1, \"type\": \"root\", \"nodes\": [{\"id\": 2, \"type\": \"output\", \"name\": \"eDP-1\","
         "\"nodes\": [{\"id\": 3, \"type\": \"workspace\", \"name\": \"1\", \"nodes\": ["
         "{\"id\": 10, \"pid\": 100, \"app_id\": \"one\"}]}]}]}";

struct attention_test_fixture {
    int compositor_socket;
    int last_count;
    int change_count;
};

static void
record_count(
    void *user_data,
    int   urgent_window_count
)
{
    struct attention_test_fixture *fixture = user_data;

    fixture->last_count = urgent_window_count;

    fixture->change_count++;
}

static const struct compositor_attention_requests_handler recording_handler = {
    .handle_count_changed = record_count,
};

static void
compositor_sends(
    struct attention_test_fixture *fixture,
    uint32_t                       message_type,
    const char                    *payload
)
{
    g_autoptr(GBytes) message = compositor_ipc_encode_message(message_type, payload);

    gsize       size  = 0;
    const void *bytes = g_bytes_get_data(message, &size);

    g_assert_cmpint(write(fixture->compositor_socket, bytes, size), ==, (ssize_t)size);
}

static void
iterate_main_context_briefly(void)
{
    for (int i = 0; i < 20; i++) {
        g_main_context_iteration(NULL, FALSE);
    }
}

static struct compositor_attention_requests *
start_requests(
    struct attention_test_fixture *fixture
)
{
    int sockets[2];

    g_assert_cmpint(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), ==, 0);

    fixture->compositor_socket = sockets[1];

    g_autoptr(GSocket)           socket     = g_socket_new_from_fd(sockets[0], NULL);
    g_autoptr(GSocketConnection) connection = g_socket_connection_factory_create_connection(socket);

    return compositor_attention_requests_create_for_connection(connection, &recording_handler, fixture);
}

/* Replies come in request order: the subscription, then the first tree. */
static void
compositor_answers_with_one_urgent_window(
    struct attention_test_fixture *fixture
)
{
    compositor_sends(fixture, COMPOSITOR_IPC_REQUEST_SUBSCRIBE_TO_EVENTS, "{\"success\": true}");
    compositor_sends(fixture, COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE, tree_with_urgent_window);
    iterate_main_context_briefly();
}

/* Focus events change nothing; an urgency event makes the library read the tree again. */
static void
compositor_reports_urgency_ended(
    struct attention_test_fixture *fixture
)
{
    compositor_sends(fixture, COMPOSITOR_IPC_EVENT_WINDOW_CHANGED, "{\"change\": \"focus\", \"container\": {\"id\": 10}}");
    compositor_sends(fixture, COMPOSITOR_IPC_EVENT_WINDOW_CHANGED, "{\"change\": \"urgent\", \"container\": {\"id\": 10}}");
    iterate_main_context_briefly();
    compositor_sends(fixture, COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE, tree_without_urgent_window);
    iterate_main_context_briefly();
}

/* Two changes: from unknown to one urgent window, then to none. */
static void
assert_counted_one_then_none(
    const struct attention_test_fixture *fixture
)
{
    g_assert_cmpint(fixture->last_count, ==, 0);
    g_assert_cmpint(fixture->change_count, ==, 2);
}

static void
test_urgent_windows_are_counted(void)
{
    struct attention_test_fixture         fixture  = {.last_count = -1};
    struct compositor_attention_requests *requests = start_requests(&fixture);

    compositor_answers_with_one_urgent_window(&fixture);
    compositor_reports_urgency_ended(&fixture);
    assert_counted_one_then_none(&fixture);
    compositor_attention_requests_destroy(requests);
    close(fixture.compositor_socket);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/compositor-attention-requests/counted", test_urgent_windows_are_counted},
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
