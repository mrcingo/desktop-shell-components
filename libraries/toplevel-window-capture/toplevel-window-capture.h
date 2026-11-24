#ifndef TOPLEVEL_WINDOW_CAPTURE_H
#define TOPLEVEL_WINDOW_CAPTURE_H

#include <stdint.h>

#include <gdk/gdk.h>

/*
 * Snapshots of individual windows through ext-image-copy-capture-v1, with
 * sources from ext-foreign-toplevel-image-capture-source-v1. Windows are
 * named by their foreign toplevel identifier.
 *
 * Each request delivers up to two frames: a hidden window, such as a
 * background tab, only redraws once the first capture asks it for a frame,
 * so its first frame may be stale and the second one is current.
 */

struct toplevel_window_capture;
struct toplevel_window_snapshot_request;

/*
 * Called with each captured frame, then once with NULL when the request has
 * ended and been freed. Must not cancel the request from inside the call.
 */
typedef void (*toplevel_window_snapshot_callback)(GdkTexture *snapshot, void *user_data);

/* Returns NULL if GDK is not running on Wayland. */
struct toplevel_window_capture *toplevel_window_capture_create(GdkDisplay *display);

void toplevel_window_capture_destroy(struct toplevel_window_capture *capture);

/*
 * Snapshots larger than the size limit are shrunk to fit it, keeping their
 * aspect ratio, before they become textures, so a full-size frame (about
 * 9 MB for a 1920x1200 window) is released as soon as it has been read.
 */
struct toplevel_window_snapshot_size_limit {
    uint32_t maximum_width;  // 0 for no limit
    uint32_t maximum_height; // 0 for no limit
};

/*
 * Returns NULL, without calling back, if the compositor lacks the protocols
 * or does not know the window.
 */
struct toplevel_window_snapshot_request *toplevel_window_capture_request_snapshots(
    struct toplevel_window_capture            *capture,
    const char                                *toplevel_identifier,
    struct toplevel_window_snapshot_size_limit size_limit,
    toplevel_window_snapshot_callback          callback,
    void                                      *user_data
);

/* The callback is not called again for a cancelled request. */
void toplevel_window_capture_cancel_snapshots(struct toplevel_window_snapshot_request *request);

#endif
