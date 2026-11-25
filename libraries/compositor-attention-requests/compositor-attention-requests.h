#ifndef COMPOSITOR_ATTENTION_REQUESTS_H
#define COMPOSITOR_ATTENTION_REQUESTS_H

#include <gio/gio.h>

/*
 * How many windows ask for attention (are marked urgent by the compositor),
 * followed through the compositor's IPC socket, and focusing the latest
 * of them. The window tree is read again only when a window's urgency
 * changes or a window closes.
 */

struct compositor_attention_requests;

struct compositor_attention_requests_handler {
    /* Only called when the count differs from the last one. */
    void (*handle_count_changed)(void *user_data, int urgent_window_count);
};

struct compositor_attention_requests *compositor_attention_requests_connect(
    const char                                         *socket_path,
    const struct compositor_attention_requests_handler *handler,
    void                                               *user_data,
    GError                                            **error
);

/* Takes over an already connected socket, as tests do. */
struct compositor_attention_requests *compositor_attention_requests_create_for_connection(
    GSocketConnection                                  *connection,
    const struct compositor_attention_requests_handler *handler,
    void                                               *user_data
);

void compositor_attention_requests_destroy(struct compositor_attention_requests *requests);

int compositor_attention_requests_get_count(const struct compositor_attention_requests *requests);

/* Focuses the window that most recently asked for attention. */
void compositor_attention_requests_focus_latest(struct compositor_attention_requests *requests);

#endif
