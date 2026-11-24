#include "layer-shell-window.h"

/* The exclusive zone GTK layer shell uses for "ignore other reservations". */
enum {
    EXCLUSIVE_ZONE_IGNORING_OTHERS = -1,
};

static const struct {
    enum layer_shell_window_edges edge;
    GtkLayerShellEdge             layer_shell_edge;
} edge_table[] = {
    {LAYER_SHELL_WINDOW_EDGE_TOP,    GTK_LAYER_SHELL_EDGE_TOP   },
    {LAYER_SHELL_WINDOW_EDGE_BOTTOM, GTK_LAYER_SHELL_EDGE_BOTTOM},
    {LAYER_SHELL_WINDOW_EDGE_LEFT,   GTK_LAYER_SHELL_EDGE_LEFT  },
    {LAYER_SHELL_WINDOW_EDGE_RIGHT,  GTK_LAYER_SHELL_EDGE_RIGHT },
};

/* Indexed by enum layer_shell_window_space_reservation. */
static const int exclusive_zone_table[] = {
    [LAYER_SHELL_WINDOW_RESERVE_NO_SPACE]          = 0,
    [LAYER_SHELL_WINDOW_RESERVE_OWN_SIZE]          = 0,
    [LAYER_SHELL_WINDOW_IGNORE_OTHER_RESERVATIONS] = EXCLUSIVE_ZONE_IGNORING_OTHERS,
};

static void
anchor_edges(
    GtkWindow   *window,
    unsigned int anchored_edges
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(edge_table); i++) {
        bool is_anchored = (anchored_edges & edge_table[i].edge) != 0;

        gtk_layer_set_anchor(window, edge_table[i].layer_shell_edge, is_anchored);
    }
}

/* Reserving the window's own size follows the window as it resizes. */
static void
reserve_space(
    GtkWindow                                *window,
    enum layer_shell_window_space_reservation space_reservation
)
{
    if (space_reservation == LAYER_SHELL_WINDOW_RESERVE_OWN_SIZE) {
        gtk_layer_auto_exclusive_zone_enable(window);
        return;
    }

    gtk_layer_set_exclusive_zone(window, exclusive_zone_table[space_reservation]);
}

static void
configure_layer_surface(
    GtkWindow                               *window,
    const struct layer_shell_window_options *options
)
{
    gtk_layer_init_for_window(window);
    gtk_layer_set_namespace(window, options->surface_namespace);
    gtk_layer_set_layer(window, options->layer);
    gtk_layer_set_monitor(window, options->monitor);
    gtk_layer_set_keyboard_mode(window, options->keyboard_mode);
}

GtkWindow *
layer_shell_window_create(
    GtkApplication                          *application,
    const struct layer_shell_window_options *options
)
{
    GtkWindow *window = GTK_WINDOW(gtk_window_new());

    gtk_window_set_application(window, application);
    configure_layer_surface(window, options);
    anchor_edges(window, options->anchored_edges);
    reserve_space(window, options->space_reservation);

    return window;
}
