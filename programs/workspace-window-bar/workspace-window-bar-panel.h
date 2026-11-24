#ifndef WORKSPACE_WINDOW_BAR_PANEL_H
#define WORKSPACE_WINDOW_BAR_PANEL_H

#include <stdint.h>

#include <gtk/gtk.h>

#include "compositor-window-tree.h"

/*
 * The bar along the bottom edge of one monitor, listing the windows of the
 * workspace shown there as "name pid". The focused window is highlighted
 * and clicking a window focuses it.
 */

struct workspace_window_bar_panel_handler {
    void (*handle_window_clicked)(void *user_data, int64_t container_id);
};

struct workspace_window_bar_panel;

struct workspace_window_bar_panel *workspace_window_bar_panel_create(
    GtkApplication                                  *application,
    GdkMonitor                                      *monitor,
    const struct workspace_window_bar_panel_handler *handler,
    void                                            *user_data
);

void workspace_window_bar_panel_destroy(struct workspace_window_bar_panel *panel);

/* The connector name of the panel's monitor, such as eDP-1. */
const char *workspace_window_bar_panel_get_output_name(struct workspace_window_bar_panel *panel);

/* output is NULL while the compositor does not know the monitor. */
void workspace_window_bar_panel_show_windows(struct workspace_window_bar_panel *panel, const struct compositor_output *output);

#endif
