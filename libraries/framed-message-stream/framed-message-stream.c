#include "framed-message-stream.h"

struct framed_message_stream {
    GSocketConnection                          *connection;
    const struct framed_message_format         *format;
    const struct framed_message_stream_handler *handler;
    void                                       *user_data;

    GCancellable *pending_operations_cancellable;

    GQueue frames_waiting_to_be_written; // GBytes, the head is being written
    bool   is_closed;

    /* Destroying from inside a callback is deferred until it returns. */
    int  callback_nesting_depth;
    bool destroy_was_requested;
};

/*
 * Asynchronous operations own their buffers and only touch the stream when
 * they were not cancelled, so a destroyed stream can be freed right away.
 */
struct pending_read_operation {
    struct framed_message_stream *stream;
    uint8_t                       header_bytes[FRAMED_MESSAGE_MAXIMUM_HEADER_SIZE];
    uint32_t                      payload_length;
    uint32_t                      message_type;
    char                         *payload;
};

struct pending_write_operation {
    struct framed_message_stream *stream;
    GBytes                       *frame;
};

static void
free_stream_memory(
    struct framed_message_stream *stream
)
{
    g_queue_clear_full(&stream->frames_waiting_to_be_written, (GDestroyNotify)g_bytes_unref);
    g_object_unref(stream->pending_operations_cancellable);
    g_object_unref(stream->connection);
    g_free(stream);
}

static void
enter_handler_callback(
    struct framed_message_stream *stream
)
{
    stream->callback_nesting_depth++;
}

/* Returns false if the stream was destroyed during the callback. */
static bool
leave_handler_callback(
    struct framed_message_stream *stream
)
{
    stream->callback_nesting_depth--;

    if (!stream->destroy_was_requested) {
        return true;
    }

    if (stream->callback_nesting_depth == 0) {
        free_stream_memory(stream);
    }

    return false;
}

static void
drop_unwritten_frames(
    struct framed_message_stream *stream
)
{
    g_queue_clear_full(&stream->frames_waiting_to_be_written, (GDestroyNotify)g_bytes_unref);
}

static void
close_stream_and_notify(
    struct framed_message_stream *stream,
    const GError                 *error
)
{
    if (stream->is_closed) {
        return;
    }

    stream->is_closed = true;

    drop_unwritten_frames(stream);

    enter_handler_callback(stream);
    stream->handler->handle_stream_closed(stream->user_data, error);
    leave_handler_callback(stream);
}

static bool
is_cancellation_error(
    const GError *error
)
{
    return g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
}

static GInputStream *
connection_input(
    struct framed_message_stream *stream
)
{
    return g_io_stream_get_input_stream(G_IO_STREAM(stream->connection));
}

static GOutputStream *
connection_output(
    struct framed_message_stream *stream
)
{
    return g_io_stream_get_output_stream(G_IO_STREAM(stream->connection));
}

static void start_reading_next_header(struct framed_message_stream *stream);

/* Returns false if the stream was closed or destroyed meanwhile. */
static bool
deliver_received_message(
    struct pending_read_operation *read
)
{
    struct framed_message_stream *stream = read->stream;

    read->payload[read->payload_length] = '\0';

    enter_handler_callback(stream);
    stream->handler->handle_received_message(stream->user_data, read->message_type, read->payload, read->payload_length);

    if (!leave_handler_callback(stream)) {
        return false;
    }

    return !stream->is_closed;
}

static void
free_read_operation(
    struct pending_read_operation *read
)
{
    g_free(read->payload);
    g_free(read);
}

static size_t
finish_read_operation(
    GObject      *source,
    GAsyncResult *result,
    GError      **error
)
{
    size_t bytes_read = 0;

    g_input_stream_read_all_finish(G_INPUT_STREAM(source), result, &bytes_read, error);

    return bytes_read;
}

/* A read that failed or came up short ends the stream; the peer hung up if there is no error. */
static bool
read_completed_or_close_stream(
    struct framed_message_stream *stream,
    size_t                        expected_bytes,
    size_t                        bytes_read,
    const GError                 *error
)
{
    bool read_is_complete = error == NULL && bytes_read == expected_bytes;

    if (!read_is_complete) {
        close_stream_and_notify(stream, error);
    }

    return read_is_complete;
}

static void
continue_after_payload(
    struct pending_read_operation *read,
    size_t                         bytes_read,
    const GError                  *error
)
{
    struct framed_message_stream *stream = read->stream;

    if (!read_completed_or_close_stream(stream, read->payload_length, bytes_read, error)) {
        return;
    }

    if (deliver_received_message(read)) {
        start_reading_next_header(stream);
    }
}

static void
handle_payload_read(
    GObject      *source,
    GAsyncResult *result,
    void         *data
)
{
    struct pending_read_operation *read       = data;
    GError                        *error      = NULL;
    size_t                         bytes_read = finish_read_operation(source, result, &error);

    if (!is_cancellation_error(error)) {
        continue_after_payload(read, bytes_read, error);
    }

    g_clear_error(&error);
    free_read_operation(read);
}

static bool
decode_and_check_header(
    struct pending_read_operation *read
)
{
    const struct framed_message_format *format = read->stream->format;

    if (!format->decode_header(read->header_bytes, &read->payload_length, &read->message_type)) {
        return false;
    }

    return read->payload_length <= format->maximum_payload_size_in_bytes;
}

static void
close_stream_for_invalid_header(
    struct framed_message_stream *stream
)
{
    GError *invalid_header_error = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Invalid message header");

    close_stream_and_notify(stream, invalid_header_error);
    g_error_free(invalid_header_error);
}

static bool
header_is_valid_or_close_stream(
    struct pending_read_operation *read
)
{
    if (decode_and_check_header(read)) {
        return true;
    }

    close_stream_for_invalid_header(read->stream);

    return false;
}

static void
start_reading_payload(
    struct pending_read_operation *read
)
{
    struct framed_message_stream *stream = read->stream;

    /* One spare byte for the terminating NUL. */
    read->payload = g_malloc((size_t)read->payload_length + 1);

    g_input_stream_read_all_async(
        connection_input(stream),
        read->payload,
        read->payload_length,
        G_PRIORITY_DEFAULT,
        stream->pending_operations_cancellable,
        handle_payload_read,
        read
    );
}

static bool
header_read_is_usable(
    struct pending_read_operation *read,
    size_t                         bytes_read,
    const GError                  *error
)
{
    size_t header_size = read->stream->format->header_size_in_bytes;

    if (!read_completed_or_close_stream(read->stream, header_size, bytes_read, error)) {
        return false;
    }

    return header_is_valid_or_close_stream(read);
}

/* Returns true if the payload read started and now owns the operation. */
static bool
start_payload_unless_cancelled(
    struct pending_read_operation *read,
    size_t                         bytes_read,
    const GError                  *error
)
{
    if (is_cancellation_error(error)) {
        return false;
    }

    if (!header_read_is_usable(read, bytes_read, error)) {
        return false;
    }

    start_reading_payload(read);

    return true;
}

static void
handle_header_read(
    GObject      *source,
    GAsyncResult *result,
    void         *data
)
{
    struct pending_read_operation *read                 = data;
    GError                        *error                = NULL;
    size_t                         bytes_read           = finish_read_operation(source, result, &error);
    bool                           payload_read_started = start_payload_unless_cancelled(read, bytes_read, error);

    g_clear_error(&error);

    if (!payload_read_started) {
        free_read_operation(read);
    }
}

static void
start_reading_next_header(
    struct framed_message_stream *stream
)
{
    struct pending_read_operation *read = g_new0(struct pending_read_operation, 1);

    read->stream = stream;

    g_input_stream_read_all_async(
        connection_input(stream),
        read->header_bytes,
        stream->format->header_size_in_bytes,
        G_PRIORITY_DEFAULT,
        stream->pending_operations_cancellable,
        handle_header_read,
        read
    );
}

static void start_writing_oldest_frame(struct framed_message_stream *stream);

/* Releases the operation and returns the write's error, if any. */
static GError *
finish_write_operation(
    GObject                        *source,
    GAsyncResult                   *result,
    struct pending_write_operation *write
)
{
    GError *error = NULL;

    g_output_stream_write_all_finish(G_OUTPUT_STREAM(source), result, NULL, &error);
    g_bytes_unref(write->frame);
    g_free(write);

    return error;
}

static void
continue_after_write(
    struct framed_message_stream *stream,
    const GError                 *error
)
{
    if (error != NULL) {
        close_stream_and_notify(stream, error);
        return;
    }

    if (stream->is_closed) {
        return; // a failed read already dropped the queue
    }

    g_bytes_unref(g_queue_pop_head(&stream->frames_waiting_to_be_written));
    start_writing_oldest_frame(stream);
}

static void
handle_frame_written(
    GObject      *source,
    GAsyncResult *result,
    void         *data
)
{
    struct pending_write_operation *write  = data;
    struct framed_message_stream   *stream = write->stream;
    GError                         *error  = finish_write_operation(source, result, write);

    if (!is_cancellation_error(error)) {
        continue_after_write(stream, error);
    }

    g_clear_error(&error);
}

static struct pending_write_operation *
create_write_operation(
    struct framed_message_stream *stream,
    GBytes                       *frame
)
{
    struct pending_write_operation *write = g_new(struct pending_write_operation, 1);

    write->stream = stream;
    write->frame  = g_bytes_ref(frame);

    return write;
}

static void
write_frame_asynchronously(
    struct pending_write_operation *write
)
{
    size_t      frame_size;
    const void *frame_bytes = g_bytes_get_data(write->frame, &frame_size);

    g_output_stream_write_all_async(
        connection_output(write->stream),
        frame_bytes,
        frame_size,
        G_PRIORITY_DEFAULT,
        write->stream->pending_operations_cancellable,
        handle_frame_written,
        write
    );
}

static void
start_writing_oldest_frame(
    struct framed_message_stream *stream
)
{
    GBytes *frame = g_queue_peek_head(&stream->frames_waiting_to_be_written);

    if (frame == NULL) {
        return;
    }

    write_frame_asynchronously(create_write_operation(stream, frame));
}

void
framed_message_stream_send_frame(
    struct framed_message_stream *stream,
    GBytes                       *frame
)
{
    if (stream->is_closed) {
        g_bytes_unref(frame);
        return;
    }

    g_queue_push_tail(&stream->frames_waiting_to_be_written, frame);

    bool no_write_in_progress = g_queue_get_length(&stream->frames_waiting_to_be_written) == 1;

    if (no_write_in_progress) {
        start_writing_oldest_frame(stream);
    }
}

bool
framed_message_stream_is_closed(
    struct framed_message_stream *stream
)
{
    return stream->is_closed;
}

static struct framed_message_stream *
allocate_stream(
    GSocketConnection                          *connection,
    const struct framed_message_format         *format,
    const struct framed_message_stream_handler *handler,
    void                                       *user_data
)
{
    struct framed_message_stream *stream = g_new0(struct framed_message_stream, 1);

    *stream = (struct framed_message_stream){
        .connection                     = g_object_ref(connection),
        .format                         = format,
        .handler                        = handler,
        .user_data                      = user_data,
        .pending_operations_cancellable = g_cancellable_new(),
        .frames_waiting_to_be_written   = G_QUEUE_INIT,
    };

    return stream;
}

struct framed_message_stream *
framed_message_stream_create(
    GSocketConnection                          *connection,
    const struct framed_message_format         *format,
    const struct framed_message_stream_handler *handler,
    void                                       *user_data
)
{
    g_return_val_if_fail(format->header_size_in_bytes <= FRAMED_MESSAGE_MAXIMUM_HEADER_SIZE, NULL);

    struct framed_message_stream *stream = allocate_stream(connection, format, handler, user_data);

    start_reading_next_header(stream);

    return stream;
}

/*
 * Pending operations hold their own references to the connection, so the
 * socket is closed explicitly instead of when they finish.
 */
static void
close_socket_and_cancel_operations(
    struct framed_message_stream *stream
)
{
    stream->is_closed             = true;
    stream->destroy_was_requested = true;

    g_cancellable_cancel(stream->pending_operations_cancellable);
    g_socket_close(g_socket_connection_get_socket(stream->connection), NULL);
    drop_unwritten_frames(stream);
}

void
framed_message_stream_destroy(
    struct framed_message_stream *stream
)
{
    if (stream == NULL) {
        return;
    }

    close_socket_and_cancel_operations(stream);

    if (stream->callback_nesting_depth == 0) {
        free_stream_memory(stream);
    }
}
