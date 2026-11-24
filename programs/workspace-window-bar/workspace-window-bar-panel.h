#ifndef WORKSPACE_WINDOW_BAR_PANEL_H
#define WORKSPACE_WINDOW_BAR_PANEL_H

#include <stdint.h>

#include <gtk/gtk.h>

#include "compositor-window-tree.h"
#include "workspace-window-bar-settings.h"

/*
 * The bar along one edge of one monitor, listing the windows of the
 * workspace shown there. The focused window is highlighted.
 */

struct workspace_window_bar_panel_handler {
    /* mouse_button is 0 for left, 1 for middle and 2 for right. */
    void (*handle_window_clicked)(void *user_data, int64_t container_id, int mouse_button);
};

struct workspace_window_bar_panel;

/* settings must outlive the panel. */
struct workspace_window_bar_panel *workspace_window_bar_panel_create(
    GtkApplication                                  *application,
    GdkMonitor                                      *monitor,
    const struct workspace_window_bar_settings      *settings,
    const struct workspace_window_bar_panel_handler *handler,
    void                                            *user_data
);

void workspace_window_bar_panel_destroy(struct workspace_window_bar_panel *panel);

/* The connector name of the panel's monitor, such as eDP-1. */
const char *workspace_window_bar_panel_get_output_name(struct workspace_window_bar_panel *panel);

/* output is NULL while the compositor does not know the monitor. */
void workspace_window_bar_panel_show_windows(struct workspace_window_bar_panel *panel, const struct compositor_output *output);

#endif
