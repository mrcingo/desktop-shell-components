#ifndef WINDOW_SWITCHER_OVERLAY_H
#define WINDOW_SWITCHER_OVERLAY_H

#include <stdbool.h>
#include <stdint.h>

#include <gtk/gtk.h>

/*
 * The Alt+Tab overlay: a card in the middle of a monitor with one preview
 * per window and the selected window highlighted. While shown it holds the
 * keyboard; releasing Alt, Return or a click chooses the selected window,
 * and Escape closes the overlay without choosing.
 */

struct window_switcher_entry {
    int64_t container_id;

    char *label; // "name pid"
};

struct window_switcher_overlay_handler {
    /* The overlay has already closed when these are called. */
    void (*handle_window_chosen)(void *user_data, int64_t container_id);
    void (*handle_overlay_closed)(void *user_data);
};

struct window_switcher_overlay;

struct window_switcher_overlay *window_switcher_overlay_create(
    GtkApplication                               *application,
    const struct window_switcher_overlay_handler *handler,
    void                                         *user_data
);

void window_switcher_overlay_destroy(struct window_switcher_overlay *overlay);

/* entries holds struct window_switcher_entry; the overlay copies what it needs. */
void window_switcher_overlay_show(
    struct window_switcher_overlay *overlay,
    GdkMonitor                     *monitor,
    GPtrArray                      *entries,
    guint                           selected_index
);

bool window_switcher_overlay_is_visible(struct window_switcher_overlay *overlay);

/* Moves the selection by step entries, wrapping around at either end. */
void window_switcher_overlay_move_selection(struct window_switcher_overlay *overlay, int step);

/* The size of one preview in device pixels, for capturing no more than is shown. */
void window_switcher_overlay_get_preview_pixel_size(
    struct window_switcher_overlay *overlay,
    uint32_t                       *pixel_width,
    uint32_t                       *pixel_height
);

void window_switcher_overlay_set_preview(struct window_switcher_overlay *overlay, int64_t container_id, GdkTexture *preview);

#endif
