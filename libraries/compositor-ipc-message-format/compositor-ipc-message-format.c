#include <string.h>

#include "compositor-ipc-message-format.h"

static const char compositor_ipc_magic[COMPOSITOR_IPC_MAGIC_SIZE] = {'i', '3', '-', 'i', 'p', 'c'};

static bool
decode_compositor_ipc_header(
    const uint8_t *header_bytes,
    uint32_t      *payload_length,
    uint32_t      *message_type
)
{
    if (memcmp(header_bytes, compositor_ipc_magic, COMPOSITOR_IPC_MAGIC_SIZE) != 0) {
        return false;
    }

    const uint8_t *length_bytes = header_bytes + COMPOSITOR_IPC_MAGIC_SIZE;
    const uint8_t *type_bytes   = length_bytes + sizeof(*payload_length);

    memcpy(payload_length, length_bytes, sizeof(*payload_length));
    memcpy(message_type, type_bytes, sizeof(*message_type));

    return true;
}

const struct framed_message_format compositor_ipc_message_format = {
    .header_size_in_bytes          = COMPOSITOR_IPC_HEADER_SIZE,
    .maximum_payload_size_in_bytes = COMPOSITOR_IPC_MAXIMUM_PAYLOAD_SIZE,
    .decode_header                 = decode_compositor_ipc_header,
};

static size_t
length_of_optional_text(
    const char *text
)
{
    if (text == NULL) {
        return 0;
    }

    return strlen(text);
}

static void
write_compositor_ipc_header(
    uint8_t *message,
    uint32_t message_type,
    uint32_t payload_length
)
{
    uint8_t *length_bytes = message + COMPOSITOR_IPC_MAGIC_SIZE;
    uint8_t *type_bytes   = length_bytes + sizeof(payload_length);

    memcpy(message, compositor_ipc_magic, COMPOSITOR_IPC_MAGIC_SIZE);
    memcpy(length_bytes, &payload_length, sizeof(payload_length));
    memcpy(type_bytes, &message_type, sizeof(message_type));
}

/* memcpy() must not see the NULL of an empty payload. */
static void
write_compositor_ipc_payload(
    uint8_t    *message,
    const char *payload,
    size_t      payload_length
)
{
    if (payload_length == 0) {
        return;
    }

    memcpy(message + COMPOSITOR_IPC_HEADER_SIZE, payload, payload_length);
}

GBytes *
compositor_ipc_encode_message(
    uint32_t    message_type,
    const char *payload
)
{
    size_t   payload_length = length_of_optional_text(payload);
    size_t   message_size   = COMPOSITOR_IPC_HEADER_SIZE + payload_length;
    uint8_t *message        = g_malloc(message_size);

    g_assert(payload_length <= COMPOSITOR_IPC_MAXIMUM_PAYLOAD_SIZE);
    write_compositor_ipc_header(message, message_type, (uint32_t)payload_length);
    write_compositor_ipc_payload(message, payload, payload_length);

    return g_bytes_new_take(message, message_size);
}

bool
compositor_ipc_message_type_is_event(
    uint32_t message_type
)
{
    return (message_type & COMPOSITOR_IPC_EVENT_TYPE_BIT) != 0;
}
