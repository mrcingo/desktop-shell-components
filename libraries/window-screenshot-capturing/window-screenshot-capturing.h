#ifndef WINDOW_SCREENSHOT_CAPTURING_H
#define WINDOW_SCREENSHOT_CAPTURING_H

#include <stdbool.h>

#include <gdk/gdk.h>

/*
 * Screenshots of one window, as Alt+Print Screen takes them on Windows:
 * the window's own content at full size, captured from the compositor
 * (see toplevel-window-capture.h), so windows on top of it and menus never
 * show. The picture goes to the clipboard and into a PNG file, as the
 * options say. Nothing is shown on screen; failures go to the log.
 */

struct window_screenshot_options {
    bool        copies_to_clipboard;
    bool        saves_file;
    const char *folder; // NULL or empty for Pictures/Screenshots; see window-screenshot-file-naming.h
};

struct window_screenshot_capturing;

/* The clipboard belongs to the display, so the caller should keep running while it may be pasted. */
struct window_screenshot_capturing *window_screenshot_capturing_create(GdkDisplay *display);

/* Unfinished screenshots are dropped. */
void window_screenshot_capturing_destroy(struct window_screenshot_capturing *capturing);

/*
 * Starts capturing the window with this foreign toplevel identifier.
 * Returns false when it cannot be captured at all, such as without
 * compositor support. Files are only written inside the home folder.
 */
bool window_screenshot_capturing_take(
    struct window_screenshot_capturing     *capturing,
    const char                             *capture_identifier,
    const char                             *application_name,
    const struct window_screenshot_options *options
);

#endif
