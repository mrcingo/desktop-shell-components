#ifndef WINDOW_SNAP_OVERLAY_H
#define WINDOW_SNAP_OVERLAY_H

#include <gtk/gtk.h>

/*
 * A see-through surface over one monitor while a title bar is pressed: it
 * follows the pointer, reports the release, and draws where the window
 * will land with a short label. Positions are the monitor's pixels.
 */

struct window_snap_overlay_handler {
    /* The surface is on screen and can now be given the pointer press. */
    void (*handle_shown)(void *user_data);
    void (*handle_pointer_moved)(void *user_data, double x, double y);
    void (*handle_button_released)(void *user_data, double x, double y);
    /* The right button. */
    void (*handle_cancelled)(void *user_data);
};

struct window_snap_overlay;

struct window_snap_overlay *window_snap_overlay_create(
    GtkApplication                           *application,
    GdkMonitor                               *monitor, // NULL lets the compositor choose
    const struct window_snap_overlay_handler *handler,
    void                                     *user_data
);

void window_snap_overlay_destroy(struct window_snap_overlay *overlay);

/* preview NULL draws nothing; label NULL draws no label. opacity is in percent. */
void window_snap_overlay_show_preview(
    struct window_snap_overlay *overlay,
    const GdkRectangle         *preview,
    const char                 *label,
    int                         opacity
);

#endif
