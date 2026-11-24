#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "framed-message-stream.h"

/* A test protocol: a 32-bit length header and no type. */
static bool
decode_length_header(
    const uint8_t *header_bytes,
    uint32_t      *payload_length,
    uint32_t      *message_type
)
{
    memcpy(payload_length, header_bytes, sizeof(*payload_length));

    *message_type = 0;

    return true;
}

static const struct framed_message_format length_prefixed_format = {
    .header_size_in_bytes          = sizeof(uint32_t),
    .maximum_payload_size_in_bytes = 64,
    .decode_header                 = decode_length_header,
};

struct stream_test_fixture {
    int                           peer_socket;
    struct framed_message_stream *stream;

    GPtrArray *received_payloads; // char *
    bool       was_closed;

    bool closed_with_error;
    bool destroy_on_first_message;
};

static void
record_message(
    void       *user_data,
    uint32_t    message_type,
    const char *payload,
    size_t      payload_length
)
{
    struct stream_test_fixture *fixture = user_data;

    g_ptr_array_add(fixture->received_payloads, g_strdup(payload));

    if (fixture->destroy_on_first_message) {
        framed_message_stream_destroy(fixture->stream);

        fixture->stream = NULL;
    }
}

static void
record_close(
    void         *user_data,
    const GError *error
)
{
    struct stream_test_fixture *fixture = user_data;

    fixture->was_closed        = true;
    fixture->closed_with_error = error != NULL;
}

static const struct framed_message_stream_handler recording_handler = {
    .handle_received_message = record_message,
    .handle_stream_closed    = record_close,
};

static int
create_connected_socket_pair(
    int *peer_socket
)
{
    int sockets[2];

    g_assert_cmpint(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), ==, 0);

    *peer_socket = sockets[1];

    return sockets[0];
}

static struct framed_message_stream *
create_stream_for_socket(
    int                         socket_descriptor,
    struct stream_test_fixture *fixture
)
{
    GSocket           *socket     = g_socket_new_from_fd(socket_descriptor, NULL);
    GSocketConnection *connection = g_socket_connection_factory_create_connection(socket);
    struct framed_message_stream
        *stream = framed_message_stream_create(connection, &length_prefixed_format, &recording_handler, fixture);

    g_object_unref(connection);
    g_object_unref(socket);

    return stream;
}

static void
set_up_fixture(
    struct stream_test_fixture *fixture
)
{
    int stream_socket = create_connected_socket_pair(&fixture->peer_socket);

    fixture->received_payloads = g_ptr_array_new_with_free_func(g_free);
    fixture->stream            = create_stream_for_socket(stream_socket, fixture);
}

static void
tear_down_fixture(
    struct stream_test_fixture *fixture
)
{
    framed_message_stream_destroy(fixture->stream);
    g_ptr_array_unref(fixture->received_payloads);
    close(fixture->peer_socket);
}

static void
peer_writes_message(
    struct stream_test_fixture *fixture,
    uint32_t                    declared_length,
    const char                 *payload
)
{
    size_t length = strlen(payload);

    g_assert_cmpint(write(fixture->peer_socket, &declared_length, sizeof(declared_length)), ==, sizeof(declared_length));
    g_assert_cmpint(write(fixture->peer_socket, payload, length), ==, (ssize_t)length);
}

/* payloads is NULL-terminated; each is sent with its true length. */
static void
peer_writes_messages(
    struct stream_test_fixture *fixture,
    const char *const          *payloads
)
{
    for (size_t i = 0; payloads[i] != NULL; i++) {
        peer_writes_message(fixture, (uint32_t)strlen(payloads[i]), payloads[i]);
    }
}

static void
iterate_until(
    bool *condition
)
{
    for (int i = 0; i < 1000 && !*condition; i++) {
        g_main_context_iteration(NULL, TRUE);
    }

    g_assert_true(*condition);
}

static void
iterate_until_payload_count(
    struct stream_test_fixture *fixture,
    guint                       count
)
{
    for (int i = 0; i < 1000 && fixture->received_payloads->len < count; i++) {
        g_main_context_iteration(NULL, TRUE);
    }

    g_assert_cmpuint(fixture->received_payloads->len, ==, count);
}

static void
run_pending_events(void)
{
    for (int i = 0; i < 10; i++) {
        g_main_context_iteration(NULL, FALSE);
    }
}

static void
assert_received_payloads(
    struct stream_test_fixture *fixture,
    const char *const          *expected_payloads
)
{
    for (guint i = 0; expected_payloads[i] != NULL; i++) {
        g_assert_cmpstr(g_ptr_array_index(fixture->received_payloads, i), ==, expected_payloads[i]);
    }
}

static const char *const payloads_in_order[] = {"first", "", "second", NULL};

static void
test_messages_arrive_in_order(void)
{
    struct stream_test_fixture fixture = {0};

    set_up_fixture(&fixture);
    peer_writes_messages(&fixture, payloads_in_order);
    iterate_until_payload_count(&fixture, 3);
    assert_received_payloads(&fixture, payloads_in_order);
    g_assert_false(fixture.was_closed);
    tear_down_fixture(&fixture);
}

static void
count_frame_release(
    void *data
)
{
    int *release_count = data;

    (*release_count)++;
}

static void
send_counted_frames(
    struct stream_test_fixture *fixture,
    int                        *release_count
)
{
    framed_message_stream_send_frame(fixture->stream, g_bytes_new_with_free_func("abc", 3, count_frame_release, release_count));
    framed_message_stream_send_frame(fixture->stream, g_bytes_new_with_free_func("def", 3, count_frame_release, release_count));
}

static void
iterate_until_release_count(
    int *release_count,
    int  expected_count
)
{
    for (int i = 0; i < 1000 && *release_count < expected_count; i++) {
        g_main_context_iteration(NULL, TRUE);
    }

    g_assert_cmpint(*release_count, ==, expected_count);
}

static void
assert_peer_reads(
    struct stream_test_fixture *fixture,
    const char                 *expected_bytes
)
{
    char   received_bytes[64] = {0};
    size_t expected_length    = strlen(expected_bytes);

    g_assert_cmpint(read(fixture->peer_socket, received_bytes, expected_length), ==, (ssize_t)expected_length);
    g_assert_cmpstr(received_bytes, ==, expected_bytes);
}

static void
test_frames_are_written_in_order_and_released(void)
{
    struct stream_test_fixture fixture       = {0};
    int                        release_count = 0;

    set_up_fixture(&fixture);
    send_counted_frames(&fixture, &release_count);
    iterate_until_release_count(&release_count, 2);
    assert_peer_reads(&fixture, "abcdef");
    tear_down_fixture(&fixture);
}

static void
test_peer_hang_up_closes_without_error(void)
{
    struct stream_test_fixture fixture = {0};

    set_up_fixture(&fixture);
    shutdown(fixture.peer_socket, SHUT_WR);
    iterate_until(&fixture.was_closed);
    g_assert_false(fixture.closed_with_error);
    g_assert_true(framed_message_stream_is_closed(fixture.stream));
    tear_down_fixture(&fixture);
}

static void
test_oversized_message_closes_with_error(void)
{
    struct stream_test_fixture fixture = {0};

    set_up_fixture(&fixture);
    peer_writes_message(&fixture, 65, "");
    iterate_until(&fixture.was_closed);
    g_assert_true(fixture.closed_with_error);
    g_assert_cmpuint(fixture.received_payloads->len, ==, 0);
    tear_down_fixture(&fixture);
}

/*
 * The peer sees the end of the connection at once: end of file, or a reset
 * because the second message was never read.
 */
static void
assert_stream_destroyed_and_peer_sees_end(
    struct stream_test_fixture *fixture
)
{
    char byte;

    g_assert_cmpuint(fixture->received_payloads->len, ==, 1);
    g_assert_null(fixture->stream);
    g_assert_cmpint(read(fixture->peer_socket, &byte, 1), <=, 0);
}

static const char *const two_payloads[] = {"one", "two", NULL};

static void
test_destroy_inside_callback_is_safe(void)
{
    struct stream_test_fixture fixture = {.destroy_on_first_message = true};

    set_up_fixture(&fixture);
    peer_writes_messages(&fixture, two_payloads);
    iterate_until_payload_count(&fixture, 1);
    run_pending_events();
    assert_stream_destroyed_and_peer_sees_end(&fixture);
    tear_down_fixture(&fixture);
}

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/framed-message-stream/messages-in-order", test_messages_arrive_in_order);
    g_test_add_func("/framed-message-stream/frames-written-and-released", test_frames_are_written_in_order_and_released);
    g_test_add_func("/framed-message-stream/peer-hang-up", test_peer_hang_up_closes_without_error);
    g_test_add_func("/framed-message-stream/oversized-message", test_oversized_message_closes_with_error);
    g_test_add_func("/framed-message-stream/destroy-inside-callback", test_destroy_inside_callback_is_safe);

    return g_test_run();
}
