#include "alt-key-release-watcher.h"

struct alt_key_release_watcher {
    GtkWindow *window;

    GtkEventController      *key_controller; // owned by the window
    GdkDevice               *keyboard;       // NULL on a seat without a keyboard
    alt_key_release_callback callback;

    void *user_data;
    guint activation_check_source;
};

static const guint alt_key_values[] = {
    GDK_KEY_Alt_L,
    GDK_KEY_Alt_R,
    GDK_KEY_Meta_L,
    GDK_KEY_Meta_R,
};

static gboolean
is_alt_key_value(
    guint key_value
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(alt_key_values); i++) {
        if (alt_key_values[i] == key_value) {
            return TRUE;
        }
    }

    return FALSE;
}

/* Without a keyboard nothing can release Alt, so it counts as held. */
static gboolean
is_alt_held(
    struct alt_key_release_watcher *watcher
)
{
    if (watcher->keyboard == NULL) {
        return TRUE;
    }

    GdkModifierType modifier_state = gdk_device_get_modifier_state(watcher->keyboard);

    return (modifier_state & GDK_ALT_MASK) != 0;
}

static void
report_release_if_alt_is_up(
    struct alt_key_release_watcher *watcher
)
{
    if (is_alt_held(watcher)) {
        return;
    }

    watcher->callback(watcher->user_data);
}

static void
handle_key_released(
    GtkEventControllerKey *key_controller,
    guint                  key_value,
    guint                  key_code,
    GdkModifierType        modifier_state,
    void                  *user_data
)
{
    struct alt_key_release_watcher *watcher = user_data;

    if (!is_alt_key_value(key_value)) {
        return;
    }

    watcher->callback(watcher->user_data);
}

static void
handle_modifier_state_changed(
    GObject    *keyboard,
    GParamSpec *property,
    void       *user_data
)
{
    struct alt_key_release_watcher *watcher = user_data;

    if (!gtk_window_is_active(watcher->window)) {
        return;
    }

    report_release_if_alt_is_up(watcher);
}

static gboolean
check_alt_after_activation(
    void *user_data
)
{
    struct alt_key_release_watcher *watcher = user_data;

    watcher->activation_check_source = 0;

    report_release_if_alt_is_up(watcher);

    return G_SOURCE_REMOVE;
}

/*
 * The keyboard's modifiers arrive right after the window becomes active, in
 * the same batch of events, so the check waits until the batch is handled.
 */
static void
handle_window_activation_changed(
    GObject    *window,
    GParamSpec *property,
    void       *user_data
)
{
    struct alt_key_release_watcher *watcher = user_data;

    if (!gtk_window_is_active(watcher->window)) {
        return;
    }

    if (watcher->activation_check_source == 0) {
        watcher->activation_check_source = g_idle_add(check_alt_after_activation, watcher);
    }
}

static GdkDevice *
find_keyboard(void)
{
    GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default());

    if (seat == NULL) {
        return NULL;
    }

    return gdk_seat_get_keyboard(seat);
}

static void
watch_key_releases(
    struct alt_key_release_watcher *watcher
)
{
    watcher->key_controller = gtk_event_controller_key_new();

    gtk_event_controller_set_propagation_phase(watcher->key_controller, GTK_PHASE_CAPTURE);
    g_signal_connect(watcher->key_controller, "key-released", G_CALLBACK(handle_key_released), watcher);
    gtk_widget_add_controller(GTK_WIDGET(watcher->window), watcher->key_controller);
}

static void
watch_modifier_state(
    struct alt_key_release_watcher *watcher
)
{
    if (watcher->keyboard == NULL) {
        return;
    }

    g_signal_connect(watcher->keyboard, "notify::modifier-state", G_CALLBACK(handle_modifier_state_changed), watcher);
}

static struct alt_key_release_watcher *
allocate_watcher(
    GtkWindow               *window,
    alt_key_release_callback callback,
    void                    *user_data
)
{
    struct alt_key_release_watcher *watcher = g_new0(struct alt_key_release_watcher, 1);

    watcher->window    = window;
    watcher->keyboard  = find_keyboard();
    watcher->callback  = callback;
    watcher->user_data = user_data;

    return watcher;
}

static void
watch_window_activation(
    struct alt_key_release_watcher *watcher
)
{
    g_signal_connect(watcher->window, "notify::is-active", G_CALLBACK(handle_window_activation_changed), watcher);
}

struct alt_key_release_watcher *
alt_key_release_watcher_create(
    GtkWindow               *window,
    alt_key_release_callback callback,
    void                    *user_data
)
{
    struct alt_key_release_watcher *watcher = allocate_watcher(window, callback, user_data);

    watch_key_releases(watcher);
    watch_modifier_state(watcher);
    watch_window_activation(watcher);

    return watcher;
}

static void
stop_watching_keyboard(
    struct alt_key_release_watcher *watcher
)
{
    if (watcher->keyboard == NULL) {
        return;
    }

    g_signal_handlers_disconnect_by_data(watcher->keyboard, watcher);
}

static void
stop_watching_window(
    struct alt_key_release_watcher *watcher
)
{
    g_clear_handle_id(&watcher->activation_check_source, g_source_remove);
    g_signal_handlers_disconnect_by_data(watcher->window, watcher);
    gtk_widget_remove_controller(GTK_WIDGET(watcher->window), watcher->key_controller);
}

void
alt_key_release_watcher_destroy(
    struct alt_key_release_watcher *watcher
)
{
    if (watcher == NULL) {
        return;
    }

    stop_watching_window(watcher);
    stop_watching_keyboard(watcher);
    g_free(watcher);
}
