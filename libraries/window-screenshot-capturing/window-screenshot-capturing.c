#include <unistd.h>

#include "exclusive-file-writing.h"
#include "toplevel-window-capture.h"
#include "window-screenshot-capturing.h"
#include "window-screenshot-file-naming.h"

/* Enough numbered names for any number of screenshots in the same second. */
enum {
    MOST_NAME_ATTEMPTS = 999,
};

static const mode_t screenshot_file_mode = 0600;

struct window_screenshot_capturing {
    GdkDisplay                     *display;
    struct toplevel_window_capture *window_capture; // NULL without Wayland
    GPtrArray                      *pending_screenshots;
};

struct pending_screenshot {
    struct window_screenshot_capturing      *capturing;
    struct toplevel_window_snapshot_request *request; // NULL once the capture library has ended it
    GdkTexture                              *latest_frame;
    GDateTime                               *taken_at;
    char                                    *application_name;
    char                                    *folder;
    bool                                     copies_to_clipboard;
    bool                                     saves_file;
};

static void
free_pending_screenshot(
    void *data
)
{
    struct pending_screenshot *screenshot = data;

    g_clear_object(&screenshot->latest_frame);
    g_date_time_unref(screenshot->taken_at);
    g_free(screenshot->application_name);
    g_free(screenshot->folder);
    g_free(screenshot);
}

/* -1 with a warning when the folder cannot be used. */
static int
open_screenshot_folder(
    const struct pending_screenshot *screenshot
)
{
    g_autoptr(GError) error  = NULL;
    g_autofree char  *folder = window_screenshot_resolve_folder(
        screenshot->folder,
        g_get_user_special_dir(G_USER_DIRECTORY_PICTURES),
        g_get_home_dir()
    );

    int folder_descriptor = exclusive_file_writing_open_folder(folder, g_get_home_dir(), &error);

    if (folder_descriptor < 0) {
        g_warning("Cannot save the screenshot: %s", error->message);
    }

    return folder_descriptor;
}

/* Another name only helps when this one was taken; anything else is reported and ends the attempts. */
static bool
stop_unless_name_taken(
    const GError *error
)
{
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
        return false;
    }

    g_warning("Cannot save the screenshot: %s", error->message);

    return true;
}

/* true when written, or when another attempt would not help. */
static bool
try_file_name(
    const struct pending_screenshot *screenshot,
    int                              folder_descriptor,
    GBytes                          *png_contents,
    unsigned                         attempt
)
{
    g_autoptr(GError) error     = NULL;
    g_autofree char  *file_name = window_screenshot_file_name(screenshot->taken_at, screenshot->application_name, attempt);

    if (exclusive_file_writing_write(folder_descriptor, file_name, png_contents, screenshot_file_mode, &error)) {
        return true;
    }

    return stop_unless_name_taken(error);
}

static void
save_in_folder(
    const struct pending_screenshot *screenshot,
    int                              folder_descriptor
)
{
    g_autoptr(GBytes) png_contents = gdk_texture_save_to_png_bytes(screenshot->latest_frame);

    for (unsigned attempt = 1; attempt <= MOST_NAME_ATTEMPTS; attempt++) {
        if (try_file_name(screenshot, folder_descriptor, png_contents, attempt)) {
            return;
        }
    }

    g_warning("Cannot save the screenshot: every name for this second is taken");
}

static void
save_file(
    const struct pending_screenshot *screenshot
)
{
    int folder_descriptor = open_screenshot_folder(screenshot);

    if (folder_descriptor < 0) {
        return;
    }

    save_in_folder(screenshot, folder_descriptor);
    close(folder_descriptor);
}

static void
deliver_screenshot(
    const struct pending_screenshot *screenshot
)
{
    if (screenshot->latest_frame == NULL) {
        g_warning("The window could not be captured");
        return;
    }

    if (screenshot->copies_to_clipboard) {
        gdk_clipboard_set_texture(gdk_display_get_clipboard(screenshot->capturing->display), screenshot->latest_frame);
    }

    if (screenshot->saves_file) {
        save_file(screenshot);
    }
}

/*
 * A hidden window's first frame may be stale, so every frame replaces the
 * previous one and the last is used once the request ends (NULL).
 */
static void
handle_frame(
    GdkTexture *frame,
    void       *user_data
)
{
    struct pending_screenshot *screenshot = user_data;

    if (frame != NULL) {
        g_set_object(&screenshot->latest_frame, frame);
        return;
    }

    screenshot->request = NULL;

    deliver_screenshot(screenshot);
    g_ptr_array_remove(screenshot->capturing->pending_screenshots, screenshot);
}

struct window_screenshot_capturing *
window_screenshot_capturing_create(
    GdkDisplay *display
)
{
    struct window_screenshot_capturing *capturing = g_new0(struct window_screenshot_capturing, 1);

    capturing->display             = display;
    capturing->window_capture      = toplevel_window_capture_create(display);
    capturing->pending_screenshots = g_ptr_array_new_with_free_func(free_pending_screenshot);

    return capturing;
}

static void
cancel_pending_screenshot(
    gpointer data,
    gpointer user_data
)
{
    struct pending_screenshot *screenshot = data;

    g_clear_pointer(&screenshot->request, toplevel_window_capture_cancel_snapshots);
}

void
window_screenshot_capturing_destroy(
    struct window_screenshot_capturing *capturing
)
{
    if (capturing == NULL) {
        return;
    }

    g_ptr_array_foreach(capturing->pending_screenshots, cancel_pending_screenshot, NULL);
    g_ptr_array_unref(capturing->pending_screenshots);
    g_clear_pointer(&capturing->window_capture, toplevel_window_capture_destroy);
    g_free(capturing);
}

static struct pending_screenshot *
create_pending_screenshot(
    struct window_screenshot_capturing     *capturing,
    const char                             *application_name,
    const struct window_screenshot_options *options
)
{
    struct pending_screenshot *screenshot = g_new0(struct pending_screenshot, 1);

    *screenshot = (struct pending_screenshot){
        .capturing           = capturing,
        .taken_at            = g_date_time_new_now_local(),
        .application_name    = g_strdup(application_name),
        .folder              = g_strdup(options->folder),
        .copies_to_clipboard = options->copies_to_clipboard,
        .saves_file          = options->saves_file,
    };

    return screenshot;
}

static bool
can_capture(
    struct window_screenshot_capturing *capturing,
    const char                         *capture_identifier
)
{
    if (capturing->window_capture == NULL) {
        return false;
    }

    return capture_identifier != NULL;
}

/* The capture library answers NULL, without calling back, for a window it cannot capture. */
static bool
keep_or_drop_pending(
    struct window_screenshot_capturing *capturing,
    struct pending_screenshot          *screenshot
)
{
    if (screenshot->request == NULL) {
        free_pending_screenshot(screenshot);
        return false;
    }

    g_ptr_array_add(capturing->pending_screenshots, screenshot);

    return true;
}

/* No size limit: the screenshot keeps every pixel of the window. */
bool
window_screenshot_capturing_take(
    struct window_screenshot_capturing     *capturing,
    const char                             *capture_identifier,
    const char                             *application_name,
    const struct window_screenshot_options *options
)
{
    if (!can_capture(capturing, capture_identifier)) {
        return false;
    }

    struct pending_screenshot *screenshot = create_pending_screenshot(capturing, application_name, options);
    const struct toplevel_window_snapshot_size_limit full_size = {0};

    screenshot->request = toplevel_window_capture_request_snapshots(
        capturing->window_capture,
        capture_identifier,
        full_size,
        handle_frame,
        screenshot
    );

    return keep_or_drop_pending(capturing, screenshot);
}
