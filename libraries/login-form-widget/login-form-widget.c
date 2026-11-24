#include <string.h>

#include "login-form-widget.h"

/* Keeps long questions and messages from widening the card. */
enum {
    LABEL_MAXIMUM_WIDTH_IN_CHARACTERS = 30,
};

struct login_form_widget {
    const struct login_form_widget_handler *handler;
    void                                   *user_data;
    bool                                    is_asking_question;
    bool                                    is_busy;

    GtkWidget *card;
    GtkWidget *username_entry;
    GtkWidget *answer_label;
    GtkWidget *answer_entry_stack;

    GtkWidget *secret_answer_entry;  // GtkPasswordEntry
    GtkWidget *visible_answer_entry; // GtkEntry
    GtkWidget *session_selector;

    GtkWidget *message_label;
    GtkWidget *submit_button;
};

static GtkWidget *
visible_answer_entry(
    struct login_form_widget *form
)
{
    return gtk_stack_get_visible_child(GTK_STACK(form->answer_entry_stack));
}

static const char *
entry_text(
    GtkWidget *entry
)
{
    return gtk_editable_get_text(GTK_EDITABLE(entry));
}

static void
update_sensitivity(
    struct login_form_widget *form
)
{
    bool credentials_are_editable = !form->is_busy && !form->is_asking_question;

    gtk_widget_set_sensitive(form->username_entry, credentials_are_editable);
    gtk_widget_set_sensitive(form->session_selector, credentials_are_editable);
    gtk_widget_set_sensitive(form->answer_entry_stack, !form->is_busy);
    gtk_widget_set_sensitive(form->submit_button, !form->is_busy);
}

static guint
selected_session_index(
    struct login_form_widget *form
)
{
    return gtk_drop_down_get_selected(GTK_DROP_DOWN(form->session_selector));
}

static void
send_login_request(
    struct login_form_widget *form,
    const char               *username
)
{
    const char *password      = entry_text(form->secret_answer_entry);
    guint       session_index = selected_session_index(form);

    form->handler->handle_login_requested(form->user_data, username, password, session_index);
}

/* An empty username sends the user back to that field instead. */
static void
request_login(
    struct login_form_widget *form
)
{
    char *username = g_strstrip(g_strdup(entry_text(form->username_entry)));

    if (username[0] == '\0') {
        gtk_widget_grab_focus(form->username_entry);
    } else {
        send_login_request(form, username);
    }

    g_free(username);
}

static void
handle_submit(
    GtkWidget *widget,
    void      *user_data
)
{
    struct login_form_widget *form = user_data;

    if (form->is_busy) {
        return;
    }

    if (form->is_asking_question) {
        form->handler->handle_answer_submitted(form->user_data, entry_text(visible_answer_entry(form)));
        return;
    }

    request_login(form);
}

static void
handle_username_activated(
    GtkWidget *widget,
    void      *user_data
)
{
    struct login_form_widget *form = user_data;

    gtk_widget_grab_focus(visible_answer_entry(form));
}

static gboolean
handle_key_pressed(
    GtkEventControllerKey *controller,
    guint                  key_value,
    guint                  key_code,
    GdkModifierType        modifier_state,
    void                  *user_data
)
{
    struct login_form_widget *form = user_data;

    if (key_value != GDK_KEY_Escape) {
        return FALSE;
    }

    if (!form->is_asking_question && !form->is_busy) {
        return FALSE;
    }

    form->handler->handle_cancel_requested(form->user_data);

    return TRUE;
}

static GtkWidget *
create_wrapping_label(
    const char *text,
    const char *css_class
)
{
    GtkWidget *label = gtk_label_new(text);

    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label), LABEL_MAXIMUM_WIDTH_IN_CHARACTERS);

    if (css_class != NULL) {
        gtk_widget_add_css_class(label, css_class);
    }

    return label;
}

/* Appends a labelled field; the label names target for accessibility. */
static GtkWidget *
append_labelled_field(
    struct login_form_widget *form,
    const char               *label_text,
    GtkWidget                *field,
    GtkWidget                *target
)
{
    GtkWidget *label = create_wrapping_label(label_text, "login-form-field-label");

    gtk_label_set_mnemonic_widget(GTK_LABEL(label), target);
    gtk_box_append(GTK_BOX(form->card), label);
    gtk_box_append(GTK_BOX(form->card), field);

    return label;
}

static void
create_card(
    struct login_form_widget *form
)
{
    form->card = g_object_ref_sink(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

    gtk_widget_set_name(form->card, "login-form-card");
    gtk_widget_set_halign(form->card, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(form->card, GTK_ALIGN_CENTER);
}

static void
create_username_field(
    struct login_form_widget *form
)
{
    form->username_entry = gtk_entry_new();

    gtk_widget_add_css_class(form->username_entry, "login-form-field");
    append_labelled_field(form, "Username", form->username_entry, form->username_entry);
}

static void
add_answer_entry_to_stack(
    struct login_form_widget *form,
    GtkWidget                *entry,
    const char               *entry_name
)
{
    gtk_widget_add_css_class(entry, "login-form-field");
    gtk_stack_add_named(GTK_STACK(form->answer_entry_stack), entry, entry_name);
}

/* Secret and visible answers share one place in the form, shown one at a time. */
static void
create_answer_field(
    struct login_form_widget *form
)
{
    form->secret_answer_entry  = gtk_password_entry_new();
    form->visible_answer_entry = gtk_entry_new();
    form->answer_entry_stack   = gtk_stack_new();

    add_answer_entry_to_stack(form, form->secret_answer_entry, "secret");
    add_answer_entry_to_stack(form, form->visible_answer_entry, "visible");

    form->answer_label = append_labelled_field(form, "Password", form->answer_entry_stack, form->secret_answer_entry);
}

static void
create_session_field(
    struct login_form_widget *form,
    const char *const        *session_names
)
{
    form->session_selector = gtk_drop_down_new_from_strings(session_names);

    gtk_widget_add_css_class(form->session_selector, "login-form-field");
    append_labelled_field(form, "Session", form->session_selector, form->session_selector);
}

static void
create_message_and_button(
    struct login_form_widget *form
)
{
    form->message_label = create_wrapping_label(NULL, NULL);
    form->submit_button = gtk_button_new_with_label("Log in");

    gtk_widget_set_name(form->message_label, "login-form-message");
    gtk_widget_set_visible(form->message_label, FALSE);
    gtk_widget_set_name(form->submit_button, "login-form-submit-button");

    gtk_box_append(GTK_BOX(form->card), form->message_label);
    gtk_box_append(GTK_BOX(form->card), form->submit_button);
}

/* The capture phase makes Escape work whichever field has focus. */
static void
connect_escape_key(
    struct login_form_widget *form
)
{
    GtkEventController *key_controller = gtk_event_controller_key_new();

    gtk_event_controller_set_propagation_phase(key_controller, GTK_PHASE_CAPTURE);
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(handle_key_pressed), form);
    gtk_widget_add_controller(form->card, key_controller);
}

static void
connect_signals(
    struct login_form_widget *form
)
{
    g_signal_connect(form->username_entry, "activate", G_CALLBACK(handle_username_activated), form);
    g_signal_connect(form->secret_answer_entry, "activate", G_CALLBACK(handle_submit), form);
    g_signal_connect(form->visible_answer_entry, "activate", G_CALLBACK(handle_submit), form);
    g_signal_connect(form->submit_button, "clicked", G_CALLBACK(handle_submit), form);

    connect_escape_key(form);
}

static void
create_all_widgets(
    struct login_form_widget *form,
    const char *const        *session_names
)
{
    create_card(form);
    create_username_field(form);
    create_answer_field(form);
    create_session_field(form, session_names);
    create_message_and_button(form);
}

struct login_form_widget *
login_form_widget_create(
    const char *const                      *session_names,
    const struct login_form_widget_handler *handler,
    void                                   *user_data
)
{
    struct login_form_widget *form = g_new0(struct login_form_widget, 1);

    form->handler   = handler;
    form->user_data = user_data;

    create_all_widgets(form, session_names);
    connect_signals(form);

    return form;
}

void
login_form_widget_destroy(
    struct login_form_widget *form
)
{
    if (form == NULL) {
        return;
    }

    g_assert(gtk_widget_get_parent(form->card) == NULL);
    g_object_unref(form->card);
    g_free(form);
}

GtkWidget *
login_form_widget_get_root_widget(
    struct login_form_widget *form
)
{
    return form->card;
}

void
login_form_widget_focus_next_field(
    struct login_form_widget *form
)
{
    bool has_username = entry_text(form->username_entry)[0] != '\0';

    if (form->is_asking_question || has_username) {
        gtk_widget_grab_focus(visible_answer_entry(form));
        return;
    }

    gtk_widget_grab_focus(form->username_entry);
}

static const char *
describe_submit_button(
    struct login_form_widget *form
)
{
    if (form->is_busy) {
        return "Logging in...";
    }

    if (form->is_asking_question) {
        return "Continue";
    }

    return "Log in";
}

void
login_form_widget_set_busy(
    struct login_form_widget *form,
    bool                      is_busy
)
{
    form->is_busy = is_busy;

    gtk_button_set_label(GTK_BUTTON(form->submit_button), describe_submit_button(form));
    update_sensitivity(form);
}

static void
show_answer_entry(
    struct login_form_widget *form,
    GtkWidget                *entry
)
{
    gtk_stack_set_visible_child(GTK_STACK(form->answer_entry_stack), entry);
    gtk_label_set_mnemonic_widget(GTK_LABEL(form->answer_label), entry);
    gtk_editable_set_text(GTK_EDITABLE(form->secret_answer_entry), "");
    gtk_editable_set_text(GTK_EDITABLE(form->visible_answer_entry), "");
}

/* Questions usually end with a colon, which a label above a field omits. */
static char *
question_to_field_label(
    const char *question_text
)
{
    if (question_text[0] == '\0') {
        return g_strdup("Answer");
    }

    char  *field_label  = g_strdup(question_text);
    size_t label_length = strlen(field_label);

    if (label_length > 1 && field_label[label_length - 1] == ':') {
        field_label[label_length - 1] = '\0';
    }

    return field_label;
}

static void
show_question_label(
    struct login_form_widget *form,
    const char               *question_text
)
{
    char *field_label = question_to_field_label(question_text);

    gtk_label_set_text(GTK_LABEL(form->answer_label), field_label);
    g_free(field_label);
}

static GtkWidget *
answer_entry_for(
    struct login_form_widget *form,
    bool                      answer_is_secret
)
{
    if (answer_is_secret) {
        return form->secret_answer_entry;
    }

    return form->visible_answer_entry;
}

void
login_form_widget_ask_question(
    struct login_form_widget *form,
    const char               *question_text,
    bool                      answer_is_secret
)
{
    form->is_asking_question = true;

    show_question_label(form, question_text);
    show_answer_entry(form, answer_entry_for(form, answer_is_secret));
    login_form_widget_set_busy(form, false);
    login_form_widget_focus_next_field(form);
}

void
login_form_widget_show_message(
    struct login_form_widget *form,
    const char               *message_text,
    bool                      is_error
)
{
    gtk_label_set_text(GTK_LABEL(form->message_label), message_text);

    if (is_error) {
        gtk_widget_add_css_class(form->message_label, "error");
    } else {
        gtk_widget_remove_css_class(form->message_label, "error");
    }

    gtk_widget_set_visible(form->message_label, TRUE);
}

void
login_form_widget_clear_message(
    struct login_form_widget *form
)
{
    gtk_widget_set_visible(form->message_label, FALSE);
    gtk_label_set_text(GTK_LABEL(form->message_label), "");
}

void
login_form_widget_return_to_credentials(
    struct login_form_widget *form
)
{
    gtk_label_set_text(GTK_LABEL(form->answer_label), "Password");
    show_answer_entry(form, form->secret_answer_entry);

    form->is_asking_question = false;

    login_form_widget_set_busy(form, false);
    login_form_widget_focus_next_field(form);
}
