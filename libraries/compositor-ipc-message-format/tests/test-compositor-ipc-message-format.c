#include <string.h>

#include "compositor-ipc-message-format.h"

static void
assert_header_holds(
    const uint8_t *message_bytes,
    uint32_t       expected_payload_length,
    uint32_t       expected_message_type
)
{
    uint32_t payload_length = 0;
    uint32_t message_type   = 99;

    g_assert_true(compositor_ipc_message_format.decode_header(message_bytes, &payload_length, &message_type));
    g_assert_cmpuint(payload_length, ==, expected_payload_length);
    g_assert_cmpuint(message_type, ==, expected_message_type);
}

static void
test_command_message_decodes_back(void)
{
    GBytes        *message       = compositor_ipc_encode_message(COMPOSITOR_IPC_REQUEST_RUN_COMMAND, "nop");
    const uint8_t *message_bytes = g_bytes_get_data(message, NULL);

    g_assert_cmpuint(g_bytes_get_size(message), ==, COMPOSITOR_IPC_HEADER_SIZE + 3);
    g_assert_cmpmem(message_bytes, 6, "i3-ipc", 6);
    assert_header_holds(message_bytes, 3, COMPOSITOR_IPC_REQUEST_RUN_COMMAND);
    g_assert_cmpmem(message_bytes + COMPOSITOR_IPC_HEADER_SIZE, 3, "nop", 3);
    g_bytes_unref(message);
}

static void
test_empty_payload_leaves_only_the_header(void)
{
    GBytes *message = compositor_ipc_encode_message(COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE, NULL);

    g_assert_cmpuint(g_bytes_get_size(message), ==, COMPOSITOR_IPC_HEADER_SIZE);
    assert_header_holds(g_bytes_get_data(message, NULL), 0, COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE);
    g_bytes_unref(message);
}

static void
test_wrong_magic_is_rejected(void)
{
    uint8_t  header[COMPOSITOR_IPC_HEADER_SIZE] = "i3-ipX";
    uint32_t payload_length;
    uint32_t message_type;

    g_assert_false(compositor_ipc_message_format.decode_header(header, &payload_length, &message_type));
}

static void
test_event_types_are_recognized(void)
{
    g_assert_true(compositor_ipc_message_type_is_event(COMPOSITOR_IPC_EVENT_WINDOW_CHANGED));
    g_assert_false(compositor_ipc_message_type_is_event(COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/compositor-ipc-message-format/command-round-trip", test_command_message_decodes_back        },
    {"/compositor-ipc-message-format/empty-payload",      test_empty_payload_leaves_only_the_header},
    {"/compositor-ipc-message-format/wrong-magic",        test_wrong_magic_is_rejected             },
    {"/compositor-ipc-message-format/event-types",        test_event_types_are_recognized          },
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
