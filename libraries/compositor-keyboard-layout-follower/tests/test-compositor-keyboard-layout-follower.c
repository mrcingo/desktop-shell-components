#include <sys/socket.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "compositor-ipc-message-format.h"
#include "compositor-keyboard-layout-follower.h"

static const char
    example_rules[] = "<xkbConfigRegistry><layoutList>"
                      "<layout><configItem><name>es</name><description>Spanish</description></configItem></layout>"
                      "</layoutList></xkbConfigRegistry>";

struct follower_test_fixture {
    int   compositor_socket;
    int   layout_change_count;
    char *last_layout_name;
};

static void
record_layout(
    void       *user_data,
    const char *layout_name
)
{
    struct follower_test_fixture *fixture = user_data;

    fixture->layout_change_count++;
    g_free(fixture->last_layout_name);

    fixture->last_layout_name = g_strdup(layout_name);
}

static const struct compositor_keyboard_layout_follower_handler recording_handler = {
    .handle_layout_changed = record_layout,
};

static void
compositor_sends(
    struct follower_test_fixture *fixture,
    uint32_t                      message_type,
    const char                   *payload
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

static GSocketConnection *
create_connection_from_socket_pair(
    int *compositor_socket
)
{
    int sockets[2];

    g_assert_cmpint(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), ==, 0);

    *compositor_socket = sockets[1];

    g_autoptr(GSocket) socket = g_socket_new_from_fd(sockets[0], NULL);

    return g_socket_connection_factory_create_connection(socket);
}

static char *
write_example_rules(void)
{
    char *path = NULL;
    int   file = g_file_open_tmp("rules-XXXXXX.xml", &path, NULL);

    g_close(file, NULL);
    g_assert_true(g_file_set_contents(path, example_rules, -1, NULL));

    return path;
}

/* Replies come in request order: the subscription first, then the inputs. */
static void
compositor_answers(
    struct follower_test_fixture *fixture
)
{
    compositor_sends(fixture, COMPOSITOR_IPC_REQUEST_SUBSCRIBE_TO_EVENTS, "{\"success\": true}");
    compositor_sends(
        fixture,
        COMPOSITOR_IPC_REQUEST_GET_INPUTS,
        "[{\"type\": \"keyboard\", \"xkb_active_layout_name\": \"Spanish\"}]"
    );
    iterate_main_context_briefly();
}

static void
compositor_switches_layout(
    struct follower_test_fixture *fixture,
    const char                   *layout_description
)
{
    g_autofree char *event = g_strdup_printf(
        "{\"change\": \"xkb_layout\", \"input\": {\"type\": \"keyboard\", \"xkb_active_layout_name\": \"%s\"}}",
        layout_description
    );

    compositor_sends(fixture, COMPOSITOR_IPC_EVENT_INPUT_CHANGED, event);
    iterate_main_context_briefly();
}

static struct compositor_keyboard_layout_follower *
start_follower(
    struct follower_test_fixture *fixture,
    const char                   *rules_path
)
{
    g_autoptr(GSocketConnection) connection = create_connection_from_socket_pair(&fixture->compositor_socket);

    return compositor_keyboard_layout_follower_create_for_connection(connection, rules_path, &recording_handler, fixture);
}

static void
stop_follower(
    struct follower_test_fixture               *fixture,
    struct compositor_keyboard_layout_follower *follower,
    const char                                 *rules_path
)
{
    compositor_keyboard_layout_follower_destroy(follower);
    close(fixture->compositor_socket);
    g_remove(rules_path);
    g_free(fixture->last_layout_name);
}

static void
compositor_switches_to_same_then_unlisted_layout(
    struct follower_test_fixture *fixture
)
{
    compositor_switches_layout(fixture, "Spanish");
    compositor_switches_layout(fixture, "Klingon");
}

/* "es" from the inputs, nothing for the repeated "Spanish", then the unlisted "Klingon". */
static void
assert_layouts_followed(
    const struct follower_test_fixture               *fixture,
    const struct compositor_keyboard_layout_follower *follower
)
{
    g_assert_cmpint(fixture->layout_change_count, ==, 2);
    g_assert_cmpstr(fixture->last_layout_name, ==, "Klingon");
    g_assert_cmpstr(compositor_keyboard_layout_follower_get_layout_name(follower), ==, "Klingon");
}

/* The same layout again changes nothing; one the rules lack keeps the compositor's name. */
static void
test_layout_is_followed(void)
{
    struct follower_test_fixture fixture = {0};

    g_autofree char *rules_path = write_example_rules();

    struct compositor_keyboard_layout_follower *follower = start_follower(&fixture, rules_path);

    compositor_answers(&fixture);
    compositor_switches_to_same_then_unlisted_layout(&fixture);
    assert_layouts_followed(&fixture, follower);
    stop_follower(&fixture, follower, rules_path);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/compositor-keyboard-layout-follower/follows", test_layout_is_followed},
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
