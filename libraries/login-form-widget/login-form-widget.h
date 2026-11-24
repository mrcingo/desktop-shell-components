#ifndef LOGIN_FORM_WIDGET_H
#define LOGIN_FORM_WIDGET_H

#include <stdbool.h>

#include <gtk/gtk.h>

/*
 * A login form: username, password, session selector, a message line and a
 * submit button. It can also ask the follow-up questions an authentication
 * service sends, such as a one-time code.
 *
 * Style sheet names: the box is #login-form-card, labels above fields have
 * .login-form-field-label, fields have .login-form-field, the message line
 * is #login-form-message (with .error for errors) and the button is
 * #login-form-submit-button.
 */

struct login_form_widget_handler {
    /* The user asked to log in with the session at session_index. */
    void (*handle_login_requested)(void *user_data, const char *username, const char *password, guint session_index);

    /* The user answered the question shown by ..._ask_question(). */
    void (*handle_answer_submitted)(void *user_data, const char *answer);

    /* The user pressed Escape during a question or a running attempt. */
    void (*handle_cancel_requested)(void *user_data);
};

struct login_form_widget;

/* session_names is a NULL-terminated list shown in the session selector. */
struct login_form_widget *
login_form_widget_create(const char *const *session_names, const struct login_form_widget_handler *handler, void *user_data);

/* Destroy only after removing the root widget from its window. */
void login_form_widget_destroy(struct login_form_widget *form);

/* The form owns its root widget, which can move between windows. */
GtkWidget *login_form_widget_get_root_widget(struct login_form_widget *form);

void login_form_widget_focus_next_field(struct login_form_widget *form);

void login_form_widget_set_busy(struct login_form_widget *form, bool is_busy);

void login_form_widget_ask_question(struct login_form_widget *form, const char *question_text, bool answer_is_secret);

void login_form_widget_show_message(struct login_form_widget *form, const char *message_text, bool is_error);

void login_form_widget_clear_message(struct login_form_widget *form);

/* Returns to the username and password prompt and clears any answer. */
void login_form_widget_return_to_credentials(struct login_form_widget *form);

#endif
