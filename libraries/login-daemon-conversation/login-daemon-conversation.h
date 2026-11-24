#ifndef LOGIN_DAEMON_CONVERSATION_H
#define LOGIN_DAEMON_CONVERSATION_H

#include <stdbool.h>

/*
 * A login attempt with the greetd login daemon: create a session for a
 * user, answer the questions PAM asks, then start the chosen session. Each
 * attempt uses its own connection, and the conversation can be reused once
 * an attempt has finished.
 */

enum login_attempt_result {
    LOGIN_ATTEMPT_SUCCEEDED,
    LOGIN_ATTEMPT_FAILED,
    LOGIN_ATTEMPT_CANCELLED,
};

/*
 * Called from the GLib main loop. The callbacks may start, answer or cancel
 * attempts, but must not destroy the conversation.
 */
struct login_daemon_conversation_handler {
    /* A question from PAM; answer it with ..._answer_question(). */
    void (*handle_question)(void *user_data, const char *question_text, bool answer_is_secret);

    /* An informational or error message from PAM. */
    void (*handle_message)(void *user_data, const char *message_text, bool is_error);

    /* The attempt is over; failure_text is set for failures only. */
    void (*handle_attempt_finished)(void *user_data, enum login_attempt_result result, const char *failure_text);
};

struct login_daemon_conversation;

struct login_daemon_conversation *login_daemon_conversation_create(
    const char                                     *socket_path,
    const struct login_daemon_conversation_handler *handler,
    void                                           *user_data
);

void login_daemon_conversation_destroy(struct login_daemon_conversation *conversation);

/*
 * The password answers the first secret question only; every other question
 * goes to the handler. Returns false if an attempt is already running.
 */
bool login_daemon_conversation_start_attempt(
    struct login_daemon_conversation *conversation,
    const char                       *username,
    const char                       *password,
    char *const                      *session_command,
    char *const                      *session_environment
);

/* Does nothing unless a question is waiting for an answer. */
void login_daemon_conversation_answer_question(struct login_daemon_conversation *conversation, const char *answer);

/*
 * Abandons the attempt: politely while a question waits, otherwise by
 * closing the connection, which the daemon treats as a cancel. Does nothing
 * once the session is being started.
 */
void login_daemon_conversation_cancel_attempt(struct login_daemon_conversation *conversation);

#endif
