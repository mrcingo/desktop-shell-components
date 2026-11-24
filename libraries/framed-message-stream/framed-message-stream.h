#ifndef FRAMED_MESSAGE_STREAM_H
#define FRAMED_MESSAGE_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <gio/gio.h>

/*
 * Framed messages over a connected socket: a fixed-size header that holds
 * the payload length, then the payload. Login daemons and compositors both
 * speak protocols of this shape; a framed_message_format describes how one
 * protocol lays out its header.
 */

enum {
    FRAMED_MESSAGE_MAXIMUM_HEADER_SIZE = 16,
};

struct framed_message_format {
    size_t   header_size_in_bytes; // at most FRAMED_MESSAGE_MAXIMUM_HEADER_SIZE
    uint32_t maximum_payload_size_in_bytes;
    /* Returns false for a malformed header. May leave message_type alone. */
    bool (*decode_header)(const uint8_t *header_bytes, uint32_t *payload_length, uint32_t *message_type);
};

struct framed_message_stream_handler {
    /* payload is NUL-terminated and only valid during the call. */
    void (*handle_received_message)(void *user_data, uint32_t message_type, const char *payload, size_t payload_length);
    /*
     * No more messages will arrive and sending does nothing. error is NULL
     * if the peer hung up cleanly.
     */
    void (*handle_stream_closed)(void *user_data, const GError *error);
};

struct framed_message_stream;

/* Starts reading at once and holds a reference to connection. */
struct framed_message_stream *framed_message_stream_create(
    GSocketConnection                          *connection,
    const struct framed_message_format         *format,
    const struct framed_message_stream_handler *handler,
    void                                       *user_data
);

/*
 * Closes the socket immediately so the peer sees the end at once. Safe to
 * call from inside the handler's callbacks.
 */
void framed_message_stream_destroy(struct framed_message_stream *stream);

/*
 * Queues a complete message, header included, and takes ownership of it.
 * Frames are written in order and each is released as soon as it has been
 * written, so a frame holding a secret can wipe it in its free function.
 */
void framed_message_stream_send_frame(struct framed_message_stream *stream, GBytes *frame);

bool framed_message_stream_is_closed(struct framed_message_stream *stream);

#endif
