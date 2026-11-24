#include <string.h>

#include <gdk/wayland/gdkwayland.h>

#include "ext-image-capture-source-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"
#include "foreign-toplevel-registry.h"
#include "pixel-box-downscaling.h"
#include "shared-memory-buffer.h"
#include "toplevel-window-capture.h"
#include "wayland-pixel-format-conversion.h"

enum {
    FRAMES_PER_REQUEST = 2,

    /* A window that does not redraw never produces its second frame. */
    FRAME_TIMEOUT_IN_MILLISECONDS = 1000,
};

struct toplevel_window_capture {
    struct wl_registry                                          *wayland_registry;
    struct wl_shm                                               *shared_memory;
    struct ext_foreign_toplevel_image_capture_source_manager_v1 *source_manager;
    struct ext_image_copy_capture_manager_v1                    *copy_manager;
    struct foreign_toplevel_registry                            *toplevel_registry;
    GPtrArray                                                   *requests_in_flight;
};

struct toplevel_window_snapshot_request {
    struct toplevel_window_capture            *capture;
    toplevel_window_snapshot_callback          callback;
    void                                      *user_data;
    struct toplevel_window_snapshot_size_limit size_limit;

    struct ext_image_capture_source_v1       *capture_source;
    struct ext_image_copy_capture_session_v1 *capture_session;
    struct ext_image_copy_capture_frame_v1   *pending_frame;
    struct wl_buffer                         *frame_wayland_buffer;
    struct shared_memory_buffer              *frame_memory;

    guint frame_timeout_source;
    int   frames_delivered;
    bool  retried_after_resize;

    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t chosen_wayland_format;

    int chosen_format_rank; // WAYLAND_PIXEL_FORMAT_UNSUPPORTED_RANK until a format is offered
};

static void
release_frame_resources(
    struct toplevel_window_snapshot_request *request
)
{
    g_clear_pointer(&request->pending_frame, ext_image_copy_capture_frame_v1_destroy);
    g_clear_pointer(&request->frame_wayland_buffer, wl_buffer_destroy);
    g_clear_pointer(&request->frame_memory, shared_memory_buffer_destroy);
}

static void
free_request(
    struct toplevel_window_snapshot_request *request
)
{
    g_ptr_array_remove_fast(request->capture->requests_in_flight, request);
    g_clear_handle_id(&request->frame_timeout_source, g_source_remove);

    release_frame_resources(request);

    g_clear_pointer(&request->capture_session, ext_image_copy_capture_session_v1_destroy);
    g_clear_pointer(&request->capture_source, ext_image_capture_source_v1_destroy);
    g_free(request);
}

static void
end_request(
    struct toplevel_window_snapshot_request *request
)
{
    toplevel_window_snapshot_callback callback  = request->callback;
    void                             *user_data = request->user_data;

    free_request(request);
    callback(NULL, user_data);
}

void
toplevel_window_capture_cancel_snapshots(
    struct toplevel_window_snapshot_request *request
)
{
    if (request == NULL) {
        return;
    }

    free_request(request);
}

static gboolean
handle_frame_timeout(
    void *data
)
{
    struct toplevel_window_snapshot_request *request = data;

    request->frame_timeout_source = 0;

    g_debug("Window capture timed out after %d frames", request->frames_delivered);
    end_request(request);

    return G_SOURCE_REMOVE;
}

static void
restart_frame_timeout(
    struct toplevel_window_snapshot_request *request
)
{
    g_clear_handle_id(&request->frame_timeout_source, g_source_remove);

    request->frame_timeout_source = g_timeout_add(FRAME_TIMEOUT_IN_MILLISECONDS, handle_frame_timeout, request);
}

static size_t
frame_row_stride(
    const struct toplevel_window_snapshot_request *request
)
{
    return (size_t)request->frame_width * WAYLAND_PIXEL_FORMAT_BYTES_PER_PIXEL;
}

static size_t
frame_buffer_size(
    const struct toplevel_window_snapshot_request *request
)
{
    return frame_row_stride(request) * request->frame_height;
}

static GdkMemoryFormat
gdk_format_for_wayland_format(
    uint32_t wayland_format
)
{
    GdkMemoryFormat gdk_format = GDK_MEMORY_DEFAULT;

    wayland_pixel_format_convert_to_gdk(wayland_format, &gdk_format);

    return gdk_format;
}

/* The bytes take over the shared memory instead of copying the image. */
static GBytes *
take_frame_memory_as_bytes(
    struct toplevel_window_snapshot_request *request
)
{
    GBytes *pixel_bytes = shared_memory_buffer_convert_to_bytes(request->frame_memory);

    request->frame_memory = NULL;

    return pixel_bytes;
}

static GdkTexture *
convert_full_frame_to_texture(
    struct toplevel_window_snapshot_request *request
)
{
    GdkMemoryFormat gdk_format  = gdk_format_for_wayland_format(request->chosen_wayland_format);
    GBytes         *pixel_bytes = take_frame_memory_as_bytes(request);
    GdkTexture     *texture     = gdk_memory_texture_new(
        (int)request->frame_width,
        (int)request->frame_height,
        gdk_format,
        pixel_bytes,
        frame_row_stride(request)
    );

    g_bytes_unref(pixel_bytes);

    return texture;
}

static uint32_t
limit_or_unlimited(
    uint32_t maximum
)
{
    if (maximum == 0) {
        return UINT32_MAX;
    }

    return maximum;
}

static bool
frame_exceeds_size_limit(
    struct toplevel_window_snapshot_request *request
)
{
    if (request->frame_width > limit_or_unlimited(request->size_limit.maximum_width)) {
        return true;
    }

    return request->frame_height > limit_or_unlimited(request->size_limit.maximum_height);
}

/* Reads the shared memory once while shrinking it, then releases it at once. */
static GBytes *
downscale_frame_memory(
    struct toplevel_window_snapshot_request *request,
    uint32_t                                 target_width,
    uint32_t                                 target_height
)
{
    struct pixel_box_image frame = {
        .pixels     = request->frame_memory->mapped_bytes,
        .width      = request->frame_width,
        .height     = request->frame_height,
        .row_stride = frame_row_stride(request),
    };

    GBytes *downscaled_pixels = pixel_box_downscale(&frame, target_width, target_height);

    g_clear_pointer(&request->frame_memory, shared_memory_buffer_destroy);

    return downscaled_pixels;
}

struct snapshot_size {
    uint32_t width;
    uint32_t height;
};

static struct snapshot_size
fit_frame_into_size_limit(
    struct toplevel_window_snapshot_request *request
)
{
    struct snapshot_size fitted;

    pixel_box_fit_size(
        request->frame_width,
        request->frame_height,
        limit_or_unlimited(request->size_limit.maximum_width),
        limit_or_unlimited(request->size_limit.maximum_height),
        &fitted.width,
        &fitted.height
    );

    return fitted;
}

static GdkTexture *
convert_downscaled_frame_to_texture(
    struct toplevel_window_snapshot_request *request
)
{
    struct snapshot_size fitted  = fit_frame_into_size_limit(request);
    GBytes              *pixels  = downscale_frame_memory(request, fitted.width, fitted.height);
    GdkMemoryFormat      format  = gdk_format_for_wayland_format(request->chosen_wayland_format);
    size_t               stride  = (size_t)fitted.width * PIXEL_BOX_BYTES_PER_PIXEL;
    GdkTexture          *texture = gdk_memory_texture_new((int)fitted.width, (int)fitted.height, format, pixels, stride);

    g_bytes_unref(pixels);

    return texture;
}

static GdkTexture *
convert_frame_to_texture(
    struct toplevel_window_snapshot_request *request
)
{
    if (frame_exceeds_size_limit(request)) {
        return convert_downscaled_frame_to_texture(request);
    }

    return convert_full_frame_to_texture(request);
}

static bool capture_next_frame(struct toplevel_window_snapshot_request *request);

static void
continue_with_next_frame_or_end(
    struct toplevel_window_snapshot_request *request
)
{
    if (!capture_next_frame(request)) {
        end_request(request);
    }
}

static void
deliver_frame_texture(
    struct toplevel_window_snapshot_request *request
)
{
    GdkTexture *texture = convert_frame_to_texture(request);

    request->callback(texture, request->user_data);
    g_object_unref(texture);
}

static void
ignore_frame_transform(
    void                                   *data,
    struct ext_image_copy_capture_frame_v1 *frame,
    uint32_t                                transform
)
{}

static void
ignore_frame_damage(
    void                                   *data,
    struct ext_image_copy_capture_frame_v1 *frame,
    int32_t                                 x,
    int32_t                                 y,
    int32_t                                 width,
    int32_t                                 height
)
{}

static void
ignore_frame_presentation_time(
    void                                   *data,
    struct ext_image_copy_capture_frame_v1 *frame,
    uint32_t                                seconds_high,
    uint32_t                                seconds_low,
    uint32_t                                nanoseconds
)
{}

static void
handle_frame_ready(
    void                                   *data,
    struct ext_image_copy_capture_frame_v1 *frame
)
{
    struct toplevel_window_snapshot_request *request = data;

    deliver_frame_texture(request);
    request->frames_delivered++;

    if (request->frames_delivered == FRAMES_PER_REQUEST) {
        end_request(request);
        return;
    }

    continue_with_next_frame_or_end(request);
}

/*
 * A window resized during the capture fails its frame after the session
 * has announced the new size, so one retry gets a buffer that fits.
 */
static bool
should_retry_after_failure(
    const struct toplevel_window_snapshot_request *request,
    uint32_t                                       failure_reason
)
{
    if (failure_reason != EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS) {
        return false;
    }

    return !request->retried_after_resize;
}

static void
handle_frame_failed(
    void                                   *data,
    struct ext_image_copy_capture_frame_v1 *frame,
    uint32_t                                failure_reason
)
{
    struct toplevel_window_snapshot_request *request = data;

    g_debug("Window capture frame failed with reason %u", failure_reason);

    if (!should_retry_after_failure(request, failure_reason)) {
        end_request(request);
        return;
    }

    request->retried_after_resize = true;

    continue_with_next_frame_or_end(request);
}

static const struct ext_image_copy_capture_frame_v1_listener frame_listener = {
    .transform         = ignore_frame_transform,
    .damage            = ignore_frame_damage,
    .presentation_time = ignore_frame_presentation_time,
    .ready             = handle_frame_ready,
    .failed            = handle_frame_failed,
};

static bool
frame_size_is_reasonable(
    uint32_t frame_width,
    uint32_t frame_height
)
{
    if (frame_width == 0 || frame_height == 0) {
        return false;
    }

    if (frame_width > INT32_MAX / WAYLAND_PIXEL_FORMAT_BYTES_PER_PIXEL) {
        return false;
    }

    return frame_height <= INT32_MAX / (frame_width * WAYLAND_PIXEL_FORMAT_BYTES_PER_PIXEL);
}

/* The compositor keeps its own handle on the memory, so ours closes now. */
static struct wl_buffer *
create_wayland_buffer_for_frame(
    struct toplevel_window_snapshot_request *request
)
{
    struct wl_shm_pool *pool = wl_shm_create_pool(
        request->capture->shared_memory,
        request->frame_memory->file_descriptor,
        (int32_t)frame_buffer_size(request)
    );
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(
        pool,
        0,
        (int32_t)request->frame_width,
        (int32_t)request->frame_height,
        (int32_t)frame_row_stride(request),
        request->chosen_wayland_format
    );

    wl_shm_pool_destroy(pool);
    shared_memory_buffer_close_file_descriptor(request->frame_memory);

    return buffer;
}

static bool
create_frame_buffer(
    struct toplevel_window_snapshot_request *request
)
{
    if (!frame_size_is_reasonable(request->frame_width, request->frame_height)) {
        return false;
    }

    request->frame_memory = shared_memory_buffer_create(frame_buffer_size(request));

    if (request->frame_memory == NULL) {
        return false;
    }

    request->frame_wayland_buffer = create_wayland_buffer_for_frame(request);

    return true;
}

static struct ext_image_copy_capture_frame_v1 *
create_frame_into_buffer(
    struct toplevel_window_snapshot_request *request
)
{
    struct ext_image_copy_capture_frame_v1 *frame = ext_image_copy_capture_session_v1_create_frame(request->capture_session);

    ext_image_copy_capture_frame_v1_add_listener(frame, &frame_listener, request);
    ext_image_copy_capture_frame_v1_attach_buffer(frame, request->frame_wayland_buffer);
    ext_image_copy_capture_frame_v1_damage_buffer(frame, 0, 0, (int32_t)request->frame_width, (int32_t)request->frame_height);

    return frame;
}

/* Each frame gets a fresh buffer, as the previous one became a texture. */
static bool
capture_next_frame(
    struct toplevel_window_snapshot_request *request
)
{
    release_frame_resources(request);

    if (!create_frame_buffer(request)) {
        return false;
    }

    request->pending_frame = create_frame_into_buffer(request);

    ext_image_copy_capture_frame_v1_capture(request->pending_frame);
    restart_frame_timeout(request);

    return true;
}

static void
handle_session_buffer_size(
    void                                     *data,
    struct ext_image_copy_capture_session_v1 *session,
    uint32_t                                  frame_width,
    uint32_t                                  frame_height
)
{
    struct toplevel_window_snapshot_request *request = data;

    request->frame_width  = frame_width;
    request->frame_height = frame_height;
}

static bool
format_rank_is_better(
    int candidate_rank,
    int current_rank
)
{
    if (candidate_rank == WAYLAND_PIXEL_FORMAT_UNSUPPORTED_RANK) {
        return false;
    }

    if (current_rank == WAYLAND_PIXEL_FORMAT_UNSUPPORTED_RANK) {
        return true;
    }

    return candidate_rank < current_rank;
}

static void
handle_session_shared_memory_format(
    void                                     *data,
    struct ext_image_copy_capture_session_v1 *session,
    uint32_t                                  wayland_format
)
{
    struct toplevel_window_snapshot_request *request = data;
    int                                      rank    = wayland_pixel_format_preference_rank(wayland_format);

    if (!format_rank_is_better(rank, request->chosen_format_rank)) {
        return;
    }

    request->chosen_format_rank    = rank;
    request->chosen_wayland_format = wayland_format;
}

static void
ignore_session_dmabuf_device(
    void                                     *data,
    struct ext_image_copy_capture_session_v1 *session,
    struct wl_array                          *device
)
{}

static void
ignore_session_dmabuf_format(
    void                                     *data,
    struct ext_image_copy_capture_session_v1 *session,
    uint32_t                                  format,
    struct wl_array                          *modifiers
)
{}

static bool
start_first_frame(
    struct toplevel_window_snapshot_request *request
)
{
    if (request->chosen_format_rank == WAYLAND_PIXEL_FORMAT_UNSUPPORTED_RANK) {
        g_debug("The compositor offered no usable shared memory format");
        return false;
    }

    return capture_next_frame(request);
}

/* Sent again whenever the constraints change; the first one starts capturing. */
static void
handle_session_constraints_done(
    void                                     *data,
    struct ext_image_copy_capture_session_v1 *session
)
{
    struct toplevel_window_snapshot_request *request = data;

    if (request->pending_frame != NULL) {
        return;
    }

    if (!start_first_frame(request)) {
        end_request(request);
    }
}

static void
handle_session_stopped(
    void                                     *data,
    struct ext_image_copy_capture_session_v1 *session
)
{
    end_request(data);
}

static const struct ext_image_copy_capture_session_v1_listener session_listener = {
    .buffer_size   = handle_session_buffer_size,
    .shm_format    = handle_session_shared_memory_format,
    .dmabuf_device = ignore_session_dmabuf_device,
    .dmabuf_format = ignore_session_dmabuf_format,
    .done          = handle_session_constraints_done,
    .stopped       = handle_session_stopped,
};

static bool
compositor_supports_window_capture(
    struct toplevel_window_capture *capture
)
{
    if (capture->shared_memory == NULL) {
        return false;
    }

    if (capture->source_manager == NULL) {
        return false;
    }

    return capture->copy_manager != NULL;
}

static struct ext_foreign_toplevel_handle_v1 *
find_toplevel_or_report(
    struct toplevel_window_capture *capture,
    const char                     *toplevel_identifier
)
{
    struct ext_foreign_toplevel_handle_v1 *toplevel_handle = foreign_toplevel_registry_find_handle(
        capture->toplevel_registry,
        toplevel_identifier
    );

    if (toplevel_handle == NULL) {
        g_debug("No window with identifier %s", toplevel_identifier);
    }

    return toplevel_handle;
}

static struct ext_foreign_toplevel_handle_v1 *
find_capturable_toplevel(
    struct toplevel_window_capture *capture,
    const char                     *toplevel_identifier
)
{
    if (capture == NULL || toplevel_identifier == NULL) {
        return NULL;
    }

    if (!compositor_supports_window_capture(capture)) {
        return NULL;
    }

    return find_toplevel_or_report(capture, toplevel_identifier);
}

static struct toplevel_window_snapshot_request *
create_snapshot_request(
    struct toplevel_window_capture            *capture,
    struct toplevel_window_snapshot_size_limit size_limit,
    toplevel_window_snapshot_callback          callback,
    void                                      *user_data
)
{
    struct toplevel_window_snapshot_request *request = g_new0(struct toplevel_window_snapshot_request, 1);

    *request = (struct toplevel_window_snapshot_request){
        .capture            = capture,
        .callback           = callback,
        .user_data          = user_data,
        .size_limit         = size_limit,
        .chosen_format_rank = WAYLAND_PIXEL_FORMAT_UNSUPPORTED_RANK,
    };

    g_ptr_array_add(capture->requests_in_flight, request);

    return request;
}

static void
start_capture_session(
    struct toplevel_window_snapshot_request *request,
    struct ext_foreign_toplevel_handle_v1   *toplevel_handle
)
{
    struct toplevel_window_capture *capture = request->capture;

    request->capture_source = ext_foreign_toplevel_image_capture_source_manager_v1_create_source(
        capture->source_manager,
        toplevel_handle
    );

    request
        ->capture_session = ext_image_copy_capture_manager_v1_create_session(capture->copy_manager, request->capture_source, 0);

    ext_image_copy_capture_session_v1_add_listener(request->capture_session, &session_listener, request);
    restart_frame_timeout(request);
}

struct toplevel_window_snapshot_request *
toplevel_window_capture_request_snapshots(
    struct toplevel_window_capture            *capture,
    const char                                *toplevel_identifier,
    struct toplevel_window_snapshot_size_limit size_limit,
    toplevel_window_snapshot_callback          callback,
    void                                      *user_data
)
{
    struct ext_foreign_toplevel_handle_v1 *toplevel_handle = find_capturable_toplevel(capture, toplevel_identifier);

    if (toplevel_handle == NULL) {
        return NULL;
    }

    struct toplevel_window_snapshot_request *request = create_snapshot_request(capture, size_limit, callback, user_data);

    start_capture_session(request, toplevel_handle);

    return request;
}

static bool
interface_is_named(
    const char                *interface_name,
    const struct wl_interface *interface
)
{
    return strcmp(interface_name, interface->name) == 0;
}

static void *
bind_first_version(
    struct wl_registry        *wayland_registry,
    uint32_t                   global_name,
    const struct wl_interface *interface
)
{
    return wl_registry_bind(wayland_registry, global_name, interface, 1);
}

static struct ext_foreign_toplevel_image_capture_source_manager_v1 *
bind_source_manager(
    struct wl_registry *wayland_registry,
    uint32_t            global_name
)
{
    return bind_first_version(wayland_registry, global_name, &ext_foreign_toplevel_image_capture_source_manager_v1_interface);
}

static void
handle_global(
    void               *data,
    struct wl_registry *wayland_registry,
    uint32_t            global_name,
    const char         *interface_name,
    uint32_t            version
)
{
    struct toplevel_window_capture *capture = data;

    if (interface_is_named(interface_name, &wl_shm_interface)) {
        capture->shared_memory = bind_first_version(wayland_registry, global_name, &wl_shm_interface);
    }

    if (interface_is_named(interface_name, &ext_foreign_toplevel_image_capture_source_manager_v1_interface)) {
        capture->source_manager = bind_source_manager(wayland_registry, global_name);
    }

    if (interface_is_named(interface_name, &ext_image_copy_capture_manager_v1_interface)) {
        capture->copy_manager = bind_first_version(wayland_registry, global_name, &ext_image_copy_capture_manager_v1_interface);
    }
}

static void
ignore_global_removal(
    void               *data,
    struct wl_registry *wayland_registry,
    uint32_t            global_name
)
{}

static const struct wl_registry_listener wayland_registry_listener = {
    .global        = handle_global,
    .global_remove = ignore_global_removal,
};

static struct toplevel_window_capture *
allocate_capture(
    struct wl_display *wayland_display
)
{
    struct toplevel_window_capture *capture = g_new0(struct toplevel_window_capture, 1);

    *capture = (struct toplevel_window_capture){
        .requests_in_flight = g_ptr_array_new(),
        .toplevel_registry  = foreign_toplevel_registry_create(wayland_display),
        .wayland_registry   = wl_display_get_registry(wayland_display),
    };

    return capture;
}

/* Events arrive on GDK's default queue and are dispatched by GTK's loop. */
struct toplevel_window_capture *
toplevel_window_capture_create(
    GdkDisplay *display
)
{
    if (!GDK_IS_WAYLAND_DISPLAY(display)) {
        return NULL;
    }

    struct wl_display              *wayland_display = gdk_wayland_display_get_wl_display(display);
    struct toplevel_window_capture *capture         = allocate_capture(wayland_display);

    wl_registry_add_listener(capture->wayland_registry, &wayland_registry_listener, capture);

    return capture;
}

static void
cancel_all_requests(
    struct toplevel_window_capture *capture
)
{
    while (capture->requests_in_flight->len > 0) {
        free_request(g_ptr_array_index(capture->requests_in_flight, 0));
    }

    g_ptr_array_unref(capture->requests_in_flight);
}

static void
destroy_bound_globals(
    struct toplevel_window_capture *capture
)
{
    g_clear_pointer(&capture->source_manager, ext_foreign_toplevel_image_capture_source_manager_v1_destroy);
    g_clear_pointer(&capture->copy_manager, ext_image_copy_capture_manager_v1_destroy);
    g_clear_pointer(&capture->shared_memory, wl_shm_destroy);
    wl_registry_destroy(capture->wayland_registry);
}

void
toplevel_window_capture_destroy(
    struct toplevel_window_capture *capture
)
{
    if (capture == NULL) {
        return;
    }

    cancel_all_requests(capture);
    foreign_toplevel_registry_destroy(capture->toplevel_registry);
    destroy_bound_globals(capture);
    g_free(capture);
}
