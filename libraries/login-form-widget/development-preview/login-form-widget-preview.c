/*
 * Shows the login form in one state on a full-screen layer surface, for
 * checking the style without a login daemon. Run it inside a headless
 * compositor, see development-tools/run-headless-compositor.sh.
 *
 * Usage: login-form-widget-preview DATA_DIRECTORY [empty|error|long|question|open]
 */

#include <stdlib.h>
#include <string.h>

#include "layer-shell-window.h"
#include "login-form-widget.h"
#include "shared-style-sheets.h"

static const char                 *preview_data_directory;
static struct shared_style_sheets *preview_style_sheets; // lives as long as the preview

static const char *preview_state_name = "empty";

static void
ignore_login_request(
    void       *user_data,
    const char *username,
    const char *password,
    guint       session_index
)
{}

static void
ignore_answer(
    void       *user_data,
    const char *answer
)
{}

static void
ignore_cancel(
    void *user_data
)
{}

static const struct login_form_widget_handler ignoring_handler = {
    .handle_login_requested  = ignore_login_request,
    .handle_answer_submitted = ignore_answer,
    .handle_cancel_requested = ignore_cancel,
};

static GtkWidget *
find_session_selector(
    GtkWidget *card
)
{
    for (GtkWidget *child = gtk_widget_get_first_child(card); child != NULL; child = gtk_widget_get_next_sibling(child)) {
        if (GTK_IS_DROP_DOWN(child)) {
            return child;
        }
    }

    return NULL;
}

static void
show_wrong_password_state(
    struct login_form_widget *form
)
{
    login_form_widget_show_message(form, "Wrong username or password", true);
}

static void
show_long_message_state(
    struct login_form_widget *form
)
{
    login_form_widget_show_message(form, "The account is locked due to 3 failed logins. (10 minutes left to unlock)", true);
}

static void
show_question_state(
    struct login_form_widget *form
)
{
    login_form_widget_ask_question(form, "Verification code:", false);
}

static void
show_open_session_selector_state(
    struct login_form_widget *form
)
{
    GtkWidget *session_selector = find_session_selector(login_form_widget_get_root_widget(form));

    g_signal_emit_by_name(session_selector, "activate");
}

static const struct {
    const char *state_name;

    void (*show_state)(struct login_form_widget *form);
} preview_states[] = {
    {"error",    show_wrong_password_state       },
    {"long",     show_long_message_state         },
    {"question", show_question_state             },
    {"open",     show_open_session_selector_state},
};

/* The "empty" state, or any unknown name, leaves the form untouched. */
static gboolean
apply_preview_state(
    void *user_data
)
{
    struct login_form_widget *form = user_data;

    for (size_t i = 0; i < G_N_ELEMENTS(preview_states); i++) {
        if (strcmp(preview_state_name, preview_states[i].state_name) == 0) {
            preview_states[i].show_state(form);
        }
    }

    return G_SOURCE_REMOVE;
}

static GtkWindow *
create_preview_window(
    GtkApplication *application
)
{
    const struct layer_shell_window_options window_options = {
        .surface_namespace = "login-form-widget-preview",
        .layer             = GTK_LAYER_SHELL_LAYER_OVERLAY,
        .monitor           = NULL,
        .anchored_edges    = LAYER_SHELL_WINDOW_ALL_EDGES,
        .space_reservation = LAYER_SHELL_WINDOW_IGNORE_OTHER_RESERVATIONS,
        .keyboard_mode     = GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE,
    };

    GtkWindow *window = layer_shell_window_create(application, &window_options);

    gtk_widget_add_css_class(GTK_WIDGET(window), "login-greeter-background");

    return window;
}

static void
show_form_in_window(
    struct login_form_widget *form,
    GtkWindow                *window
)
{
    gtk_window_set_child(window, login_form_widget_get_root_widget(form));
    gtk_window_present(window);
    login_form_widget_focus_next_field(form);
}

static void
handle_activate(
    GtkApplication *application,
    void           *user_data
)
{
    static const char *const session_names[] = {"Desktop", "Shell", NULL};

    struct login_form_widget *form   = login_form_widget_create(session_names, &ignoring_handler, NULL);
    GtkWindow                *window = create_preview_window(application);

    preview_style_sheets = shared_style_sheets_create(gdk_display_get_default(), preview_data_directory, "login-greeter", NULL);

    show_form_in_window(form, window);
    g_timeout_add(300, apply_preview_state, form);
}

static GtkApplication *
create_preview_application(void)
{
    GtkApplication *application = gtk_application_new(NULL, G_APPLICATION_NON_UNIQUE);

    g_signal_connect(application, "activate", G_CALLBACK(handle_activate), NULL);

    return application;
}

static int
run_preview_application(void)
{
    GtkApplication *application = create_preview_application();
    int             exit_status = g_application_run(G_APPLICATION(application), 0, NULL);

    g_object_unref(application);

    return exit_status;
}

int
main(
    int   argument_count,
    char *arguments[]
)
{
    if (argument_count < 2) {
        g_printerr("usage: %s DATA_DIRECTORY [empty|error|long|question|open]\n", arguments[0]);
        return EXIT_FAILURE;
    }

    preview_data_directory = arguments[1];

    if (argument_count > 2) {
        preview_state_name = arguments[2];
    }

    return run_preview_application();
}
