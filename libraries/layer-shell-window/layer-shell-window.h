#ifndef LAYER_SHELL_WINDOW_H
#define LAYER_SHELL_WINDOW_H

#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>

/*
 * A GTK window shown as a wlr-layer-shell surface: a panel, an overlay or a
 * full-screen background, placed by the compositor instead of tiled.
 */

enum layer_shell_window_edges {
    LAYER_SHELL_WINDOW_EDGE_TOP    = 1 << 0,
    LAYER_SHELL_WINDOW_EDGE_BOTTOM = 1 << 1,
    LAYER_SHELL_WINDOW_EDGE_LEFT   = 1 << 2,
    LAYER_SHELL_WINDOW_EDGE_RIGHT  = 1 << 3,
    LAYER_SHELL_WINDOW_ALL_EDGES   = (1 << 4) - 1,
};

enum layer_shell_window_space_reservation {
    /* Overlap other surfaces and let windows use the space below. */
    LAYER_SHELL_WINDOW_RESERVE_NO_SPACE,
    /* Keep windows out of the window's own height or width, as panels do. */
    LAYER_SHELL_WINDOW_RESERVE_OWN_SIZE,
    /* Cover panels too, as a full-screen login screen does. */
    LAYER_SHELL_WINDOW_IGNORE_OTHER_RESERVATIONS,
};

struct layer_shell_window_options {
    const char        *surface_namespace;
    GtkLayerShellLayer layer;

    GdkMonitor  *monitor; // NULL lets the compositor choose
    unsigned int anchored_edges;

    enum layer_shell_window_space_reservation space_reservation;
    GtkLayerShellKeyboardMode                 keyboard_mode;
};

/* The window is created hidden; show it with gtk_window_present(). */
GtkWindow *layer_shell_window_create(GtkApplication *application, const struct layer_shell_window_options *options);

#endif
