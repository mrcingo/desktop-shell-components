#include <gtk/gtk.h>

#include "desktop-session-entries.h"
#include "layer-shell-window.h"
#include "login-daemon-conversation.h"
#include "login-form-widget.h"
#include "login-greeter-application.h"
#include "per-monitor-objects.h"
#include "shared-style-sheets.h"

struct login_greeter_application {
    const struct login_greeter_options *options;
    GtkApplication                     *gtk_application;
    bool                                session_was_started;

    struct login_daemon_conversation *conversation;

    struct login_form_widget   *form;             // created once GTK runs
    struct per_monitor_objects *monitor_windows;  // GtkWindow per monitor
    GtkWindow                  *window_with_form; // may be NULL
};

static void
handle_question(
    void       *user_data,
    const char *question_text,
    bool        answer_is_secret
)
{
    struct login_greeter_application *greeter = user_data;

    login_form_widget_ask_question(greeter->form, question_text, answer_is_secret);
}

static void
handle_message(
    void       *user_data,
    const char *message_text,
    bool        is_error
)
{
    struct login_greeter_application *greeter = user_data;

    login_form_widget_show_message(greeter->form, message_text, is_error);
}

/* The login daemon starts the session once the greeter exits. */
static void
finish_with_started_session(
    struct login_greeter_application *greeter
)
{
    greeter->session_was_started = true;

    g_application_quit(G_APPLICATION(greeter->gtk_application));
}

static void
show_attempt_failure(
    struct login_greeter_application *greeter,
    enum login_attempt_result         result,
    const char                       *failure_text
)
{
    if (result == LOGIN_ATTEMPT_CANCELLED) {
        login_form_widget_show_message(greeter->form, "Login cancelled", false);
        return;
    }

    login_form_widget_show_message(greeter->form, failure_text, true);
}

static void
handle_attempt_finished(
    void                     *user_data,
    enum login_attempt_result result,
    const char               *failure_text
)
{
    struct login_greeter_application *greeter = user_data;

    if (result == LOGIN_ATTEMPT_SUCCEEDED) {
        finish_with_started_session(greeter);
        return;
    }

    login_form_widget_return_to_credentials(greeter->form);
    show_attempt_failure(greeter, result, failure_text);
}

static const struct login_daemon_conversation_handler conversation_handler = {
    .handle_question         = handle_question,
    .handle_message          = handle_message,
    .handle_attempt_finished = handle_attempt_finished,
};

static const struct desktop_session_entry *
find_session_entry(
    struct login_greeter_application *greeter,
    guint                             session_index
)
{
    GPtrArray *entries = greeter->options->session_entries;

    if (session_index >= entries->len) {
        return NULL;
    }

    return g_ptr_array_index(entries, session_index);
}

/* A refused start means an attempt is already running, so the form stays usable. */
static void
start_login_attempt(
    struct login_greeter_application   *greeter,
    const char                         *username,
    const char                         *password,
    const struct desktop_session_entry *session
)
{
    bool was_started = login_daemon_conversation_start_attempt(
        greeter->conversation,
        username,
        password,
        session->command_arguments,
        session->environment_variables
    );

    if (!was_started) {
        login_form_widget_set_busy(greeter->form, false);
    }
}

/*
 * The form is marked busy before starting, as the conversation may call
 * back synchronously and reset it.
 */
static void
handle_login_requested(
    void       *user_data,
    const char *username,
    const char *password,
    guint       session_index
)
{
    struct login_greeter_application   *greeter = user_data;
    const struct desktop_session_entry *session = find_session_entry(greeter, session_index);

    if (session == NULL) {
        return;
    }

    login_form_widget_clear_message(greeter->form);
    login_form_widget_set_busy(greeter->form, true);
    start_login_attempt(greeter, username, password, session);
}

static void
handle_answer_submitted(
    void       *user_data,
    const char *answer
)
{
    struct login_greeter_application *greeter = user_data;

    login_form_widget_set_busy(greeter->form, true);
    login_daemon_conversation_answer_question(greeter->conversation, answer);
}

static void
handle_cancel_requested(
    void *user_data
)
{
    struct login_greeter_application *greeter = user_data;

    login_form_widget_set_busy(greeter->form, true);
    login_daemon_conversation_cancel_attempt(greeter->conversation);
}

static const struct login_form_widget_handler form_handler = {
    .handle_login_requested  = handle_login_requested,
    .handle_answer_submitted = handle_answer_submitted,
    .handle_cancel_requested = handle_cancel_requested,
};

static void
take_form_from_window(
    struct login_greeter_application *greeter,
    GtkWindow                        *window
)
{
    if (window == NULL) {
        return;
    }

    gtk_layer_set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    gtk_window_set_child(window, NULL);
}

static void
give_form_to_window(
    struct login_greeter_application *greeter,
    GtkWindow                        *window
)
{
    if (window == NULL) {
        return;
    }

    gtk_window_set_child(window, login_form_widget_get_root_widget(greeter->form));
    gtk_layer_set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
    login_form_widget_focus_next_field(greeter->form);
}

/* Only the window holding the form takes the keyboard. */
static void
move_form_to_window(
    struct login_greeter_application *greeter,
    GtkWindow                        *window
)
{
    if (greeter->window_with_form == window) {
        return;
    }

    take_form_from_window(greeter, greeter->window_with_form);

    greeter->window_with_form = window;

    give_form_to_window(greeter, window);
}

static void
handle_pointer_entered_window(
    GtkEventControllerMotion *motion_controller,
    double                    pointer_x,
    double                    pointer_y,
    void                     *user_data
)
{
    struct login_greeter_application *greeter = user_data;
    GtkWidget                        *window  = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(motion_controller));

    move_form_to_window(greeter, GTK_WINDOW(window));
}

static GtkWindow *
create_full_screen_window(
    struct login_greeter_application *greeter,
    GdkMonitor                       *monitor
)
{
    const struct layer_shell_window_options window_options = {
        .surface_namespace = "login-greeter",
        .layer             = GTK_LAYER_SHELL_LAYER_OVERLAY,
        .monitor           = monitor,
        .anchored_edges    = LAYER_SHELL_WINDOW_ALL_EDGES,
        .space_reservation = LAYER_SHELL_WINDOW_IGNORE_OTHER_RESERVATIONS,
        .keyboard_mode     = GTK_LAYER_SHELL_KEYBOARD_MODE_NONE,
    };

    GtkWindow *window = layer_shell_window_create(greeter->gtk_application, &window_options);

    gtk_widget_add_css_class(GTK_WIDGET(window), "login-greeter-background");

    return window;
}

static void
follow_pointer_entering_window(
    struct login_greeter_application *greeter,
    GtkWindow                        *window
)
{
    GtkEventController *motion_controller = gtk_event_controller_motion_new();

    g_signal_connect(motion_controller, "enter", G_CALLBACK(handle_pointer_entered_window), greeter);
    gtk_widget_add_controller(GTK_WIDGET(window), motion_controller);
}

/* Each window covers its whole monitor, so the pointer entering it is noticed. */
static void *
create_window_for_monitor(
    void       *user_data,
    GdkMonitor *monitor
)
{
    struct login_greeter_application *greeter = user_data;
    GtkWindow                        *window  = create_full_screen_window(greeter, monitor);

    follow_pointer_entering_window(greeter, window);
    gtk_window_present(window);

    return window;
}

static void
destroy_monitor_window(
    void *user_data,
    void *object
)
{
    struct login_greeter_application *greeter = user_data;
    GtkWindow                        *window  = object;

    if (window == greeter->window_with_form) {
        move_form_to_window(greeter, NULL);
    }

    gtk_window_destroy(window);
}

/* The first window takes the form at start and when the one holding it goes away. */
static void
give_form_to_first_window(
    void                       *user_data,
    struct per_monitor_objects *monitor_windows
)
{
    struct login_greeter_application *greeter = user_data;

    if (greeter->window_with_form != NULL) {
        return;
    }

    if (per_monitor_objects_count(monitor_windows) == 0) {
        return;
    }

    move_form_to_window(greeter, per_monitor_objects_get_object(monitor_windows, 0));
}

static const struct per_monitor_objects_handler monitor_windows_handler = {
    .create_object_for_monitor = create_window_for_monitor,
    .destroy_object            = destroy_monitor_window,
    .handle_monitors_changed   = give_form_to_first_window,
};

/* A NULL-terminated list borrowing the names from the entries; free only the list. */
static const char **
list_session_names(
    GPtrArray *session_entries
)
{
    const char **session_names = g_new0(const char *, session_entries->len + 1);

    for (guint i = 0; i < session_entries->len; i++) {
        const struct desktop_session_entry *entry = g_ptr_array_index(session_entries, i);

        session_names[i] = entry->display_name;
    }

    return session_names;
}

static struct login_form_widget *
create_login_form(
    struct login_greeter_application *greeter
)
{
    const char              **session_names = list_session_names(greeter->options->session_entries);
    struct login_form_widget *form          = login_form_widget_create(session_names, &form_handler, greeter);

    g_free(session_names);

    return form;
}

static void
load_greeter_style_sheets(
    struct login_greeter_application *greeter,
    GdkDisplay                       *display
)
{
    const char *data_directory            = greeter->options->data_directory;
    const char *style_sheet_override_path = greeter->options->style_sheet_override_path;

    shared_style_sheets_load_for_display(display, data_directory, "login-greeter", style_sheet_override_path);
}

static void
create_form_and_monitor_windows(
    struct login_greeter_application *greeter,
    GdkDisplay                       *display
)
{
    greeter->form            = create_login_form(greeter);
    greeter->monitor_windows = per_monitor_objects_create(display, &monitor_windows_handler, greeter);
}

/* Without layer shell the greeter cannot cover the screen, so it quits. */
static bool
compositor_supports_layer_shell(
    GApplication *gtk_application
)
{
    if (gtk_layer_is_supported()) {
        return true;
    }

    g_printerr("The compositor does not support wlr-layer-shell\n");
    g_application_quit(gtk_application);

    return false;
}

/* Widgets can only be created once GtkApplication has started GTK. */
static void
handle_activate(
    GApplication *gtk_application,
    void         *user_data
)
{
    struct login_greeter_application *greeter = user_data;
    GdkDisplay                       *display = gdk_display_get_default();

    if (!compositor_supports_layer_shell(gtk_application)) {
        return;
    }

    load_greeter_style_sheets(greeter, display);
    create_form_and_monitor_windows(greeter, display);

    /* Windows come and go with monitors; keep running without any. */
    g_application_hold(gtk_application);
}

/* Windows must go while GTK still runs, before the form is destroyed. */
static void
handle_shutdown(
    GApplication *gtk_application,
    void         *user_data
)
{
    struct login_greeter_application *greeter = user_data;

    if (greeter->monitor_windows == NULL) {
        return;
    }

    g_clear_pointer(&greeter->monitor_windows, per_monitor_objects_destroy);
    g_application_release(gtk_application);
}

static void
run_gtk_application(
    struct login_greeter_application *greeter
)
{
    g_signal_connect(greeter->gtk_application, "activate", G_CALLBACK(handle_activate), greeter);
    g_signal_connect(greeter->gtk_application, "shutdown", G_CALLBACK(handle_shutdown), greeter);
    g_application_run(G_APPLICATION(greeter->gtk_application), 0, NULL);
}

static void
release_greeter_resources(
    struct login_greeter_application *greeter
)
{
    login_daemon_conversation_destroy(greeter->conversation);
    login_form_widget_destroy(greeter->form);
    g_object_unref(greeter->gtk_application);
}

bool
login_greeter_application_run(
    const struct login_greeter_options *options
)
{
    g_return_val_if_fail(options->session_entries->len > 0, false);

    struct login_greeter_application greeter = {
        .options         = options,
        .gtk_application = gtk_application_new(NULL, G_APPLICATION_NON_UNIQUE),
    };

    greeter.conversation = login_daemon_conversation_create(options->login_daemon_socket_path, &conversation_handler, &greeter);

    run_gtk_application(&greeter);
    release_greeter_resources(&greeter);

    return greeter.session_was_started;
}
