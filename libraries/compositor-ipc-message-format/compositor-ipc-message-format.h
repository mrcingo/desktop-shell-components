#ifndef COMPOSITOR_IPC_MESSAGE_FORMAT_H
#define COMPOSITOR_IPC_MESSAGE_FORMAT_H

#include <stdint.h>

#include <glib.h>

#include "framed-message-stream.h"

/*
 * The wire format of the i3-compatible compositor IPC that sway speaks, see
 * sway-ipc(7): the magic string "i3-ipc", a 32-bit payload length and a
 * 32-bit message type, both in native byte order, then a JSON payload.
 * Events have the high bit of their type set.
 */

enum {
    COMPOSITOR_IPC_MAGIC_SIZE           = 6,
    COMPOSITOR_IPC_HEADER_SIZE          = COMPOSITOR_IPC_MAGIC_SIZE + 2 * sizeof(uint32_t),
    COMPOSITOR_IPC_MAXIMUM_PAYLOAD_SIZE = 64 << 20,
};

enum compositor_ipc_request_type {
    COMPOSITOR_IPC_REQUEST_RUN_COMMAND         = 0,
    COMPOSITOR_IPC_REQUEST_SUBSCRIBE_TO_EVENTS = 2,
    COMPOSITOR_IPC_REQUEST_GET_WINDOW_TREE     = 4,
    COMPOSITOR_IPC_REQUEST_GET_BAR_CONFIG      = 6,
    COMPOSITOR_IPC_REQUEST_GET_CONFIGURATION   = 9,
};

/* Macros, as C11 enumerators cannot exceed INT_MAX. */
#define COMPOSITOR_IPC_EVENT_TYPE_BIT                 UINT32_C(0x80000000)
#define COMPOSITOR_IPC_EVENT_WORKSPACE_CHANGED        UINT32_C(0x80000000)
#define COMPOSITOR_IPC_EVENT_OUTPUT_CHANGED           UINT32_C(0x80000001)
#define COMPOSITOR_IPC_EVENT_WINDOW_CHANGED           UINT32_C(0x80000003)
#define COMPOSITOR_IPC_EVENT_KEY_BINDING_TRIGGERED    UINT32_C(0x80000005)
#define COMPOSITOR_IPC_EVENT_COMPOSITOR_SHUTTING_DOWN UINT32_C(0x80000006)

extern const struct framed_message_format compositor_ipc_message_format;

/* payload may be NULL for an empty payload. */
GBytes *compositor_ipc_encode_message(uint32_t message_type, const char *payload);

bool compositor_ipc_message_type_is_event(uint32_t message_type);

#endif
