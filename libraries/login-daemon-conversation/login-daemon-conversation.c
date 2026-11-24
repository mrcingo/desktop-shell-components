#define _DEFAULT_SOURCE // explicit_bzero(), provided by glibc and musl

#include <string.h>

#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>

#include "framed-message-stream.h"
#include "login-daemon-conversation.h"
#include "login-daemon-protocol.h"

enum conversation_state {
    STATE_IDLE,
    STATE_CONNECTING,
    STATE_AUTHENTICATING,   // waiting for the reply to a request or answer
    STATE_WAITING_ANSWER,   // waiting for ..._answer_question() or a cancel
    STATE_STARTING_SESSION, // waiting for the reply to start_session
    STATE_CANCELLING,       // waiting for the reply to cancel_session
};

/*
 * What one attempt needs, cleared when it ends. The password is NULL once
 * sent; the last error message from PAM and the failure text reported after
 * a cancel may be NULL.
 */
struct login_attempt_details {
    char                     *username;
    char                     *password;
    char                    **session_command;
    char                    **session_environment;
    char                     *last_error_message;
    enum login_attempt_result result_after_cancel;
    char                     *failure_text_after_cancel;
};

struct login_daemon_conversation {
    char                                           *socket_path;
    const struct login_daemon_conversation_handler *handler;
    void                                           *user_data;
    enum conversation_state                         state;
    GCancellable                                   *connect_cancellable;
    struct framed_message_stream                   *stream;
    struct login_attempt_details                    attempt;
};

typedef void (*reply_handler)(struct login_daemon_conversation *conversation, const struct login_daemon_reply *reply);

static void
wipe_and_free_secret(
    char **secret
)
{
    if (*secret == NULL) {
        return;
    }

    explicit_bzero(*secret, strlen(*secret));
    g_clear_pointer(secret, g_free);
}

static void
clear_attempt_details(
    struct login_attempt_details *attempt
)
{
    wipe_and_free_secret(&attempt->password);
    g_clear_pointer(&attempt->username, g_free);
    g_clear_pointer(&attempt->session_command, g_strfreev);
    g_clear_pointer(&attempt->session_environment, g_strfreev);
    g_clear_pointer(&attempt->last_error_message, g_free);
    g_clear_pointer(&attempt->failure_text_after_cancel, g_free);
}

static void
cancel_pending_connection(
    struct login_daemon_conversation *conversation
)
{
    if (conversation->connect_cancellable == NULL) {
        return;
    }

    g_cancellable_cancel(conversation->connect_cancellable);
    g_clear_object(&conversation->connect_cancellable);
}

/*
 * Destroying the stream closes the socket at once, so the daemon sees the
 * end of the attempt immediately.
 */
static void
end_attempt(
    struct login_daemon_conversation *conversation
)
{
    conversation->state = STATE_IDLE;

    cancel_pending_connection(conversation);
    g_clear_pointer(&conversation->stream, framed_message_stream_destroy);
    clear_attempt_details(&conversation->attempt);
}

/* Nothing may touch the conversation after the handler has been told. */
static void
finish_attempt(
    struct login_daemon_conversation *conversation,
    enum login_attempt_result         result,
    const char                       *failure_text
)
{
    char                                           *text_copy = g_strdup(failure_text);
    const struct login_daemon_conversation_handler *handler   = conversation->handler;
    void                                           *user_data = conversation->user_data;

    end_attempt(conversation);
    handler->handle_attempt_finished(user_data, result, text_copy);
    g_free(text_copy);
}

static void
fail_attempt(
    struct login_daemon_conversation *conversation,
    const GError                     *error
)
{
    bool was_connected = conversation->state != STATE_CONNECTING;

    g_warning("Login daemon: %s", error->message);

    if (was_connected) {
        finish_attempt(conversation, LOGIN_ATTEMPT_FAILED, "Login service error");
        return;
    }

    finish_attempt(conversation, LOGIN_ATTEMPT_FAILED, "Cannot reach the login service");
}

static void
fail_attempt_with_message(
    struct login_daemon_conversation *conversation,
    GQuark                            error_domain,
    int                               error_code,
    const char                       *message
)
{
    GError *error = g_error_new_literal(error_domain, error_code, message);

    fail_attempt(conversation, error);
    g_error_free(error);
}

static void
send_request(
    struct login_daemon_conversation *conversation,
    GBytes                           *request
)
{
    framed_message_stream_send_frame(conversation->stream, request);
}

static void
cancel_with_result(
    struct login_daemon_conversation *conversation,
    enum login_attempt_result         result,
    const char                       *failure_text
)
{
    conversation->state                             = STATE_CANCELLING;
    conversation->attempt.result_after_cancel       = result;
    conversation->attempt.failure_text_after_cancel = g_strdup(failure_text);

    send_request(conversation, login_daemon_encode_cancel_session_request());
}

static void
send_password(
    struct login_daemon_conversation *conversation
)
{
    GBytes *request = login_daemon_encode_authentication_response(conversation->attempt.password);

    wipe_and_free_secret(&conversation->attempt.password);
    send_request(conversation, request);
}

static void
remember_error_message(
    struct login_daemon_conversation *conversation,
    const char                       *message_text
)
{
    g_free(conversation->attempt.last_error_message);

    conversation->attempt.last_error_message = g_strdup(message_text);
}

static void
show_message(
    struct login_daemon_conversation *conversation,
    const char                       *message_text,
    bool                              is_error
)
{
    if (message_text[0] == '\0') {
        return;
    }

    conversation->handler->handle_message(conversation->user_data, message_text, is_error);

    if (is_error) {
        remember_error_message(conversation, message_text);
    }
}

static void
ask_question(
    struct login_daemon_conversation *conversation,
    const struct login_daemon_reply  *reply
)
{
    bool is_secret = reply->message_kind == LOGIN_DAEMON_MESSAGE_SECRET_QUESTION;

    if (is_secret && conversation->attempt.password != NULL) {
        send_password(conversation);
        return;
    }

    conversation->state = STATE_WAITING_ANSWER;

    conversation->handler->handle_question(conversation->user_data, reply->text, is_secret);
}

static bool
is_question(
    const struct login_daemon_reply *reply
)
{
    if (reply->message_kind == LOGIN_DAEMON_MESSAGE_SECRET_QUESTION) {
        return true;
    }

    return reply->message_kind == LOGIN_DAEMON_MESSAGE_VISIBLE_QUESTION;
}

/* Information and errors are shown, then acknowledged with an empty response. */
static void
handle_authentication_message(
    struct login_daemon_conversation *conversation,
    const struct login_daemon_reply  *reply
)
{
    if (is_question(reply)) {
        ask_question(conversation, reply);
        return;
    }

    show_message(conversation, reply->text, reply->message_kind == LOGIN_DAEMON_MESSAGE_ERROR);
    send_request(conversation, login_daemon_encode_authentication_response(NULL));
}

static const char *
describe_failure(
    const struct login_daemon_reply *reply,
    const char                      *fallback_text
)
{
    if (reply->is_authentication_failure) {
        return "Wrong username or password";
    }

    if (reply->text[0] == '\0') {
        return fallback_text;
    }

    return reply->text;
}

/* PAM's own explanation, such as a locked account, comes first. */
static const char *
describe_authentication_failure(
    struct login_daemon_conversation *conversation,
    const struct login_daemon_reply  *reply
)
{
    if (conversation->attempt.last_error_message != NULL) {
        return conversation->attempt.last_error_message;
    }

    return describe_failure(reply, "Login failed");
}

static void
start_session(
    struct login_daemon_conversation *conversation
)
{
    struct login_attempt_details *attempt = &conversation->attempt;

    conversation->state = STATE_STARTING_SESSION;

    send_request(
        conversation,
        login_daemon_encode_start_session_request(attempt->session_command, attempt->session_environment)
    );
}

static void
handle_reply_while_authenticating(
    struct login_daemon_conversation *conversation,
    const struct login_daemon_reply  *reply
)
{
    if (reply->kind == LOGIN_DAEMON_REPLY_AUTHENTICATION_MESSAGE) {
        handle_authentication_message(conversation, reply);
        return;
    }

    if (reply->kind == LOGIN_DAEMON_REPLY_SUCCESS) {
        start_session(conversation);
        return;
    }

    cancel_with_result(conversation, LOGIN_ATTEMPT_FAILED, describe_authentication_failure(conversation, reply));
}

static const char *
describe_session_start_failure(
    const struct login_daemon_reply *reply
)
{
    if (reply->kind == LOGIN_DAEMON_REPLY_AUTHENTICATION_MESSAGE) {
        return "Unexpected question from the login service";
    }

    return describe_failure(reply, "Cannot start the session");
}

static void
handle_reply_while_starting_session(
    struct login_daemon_conversation *conversation,
    const struct login_daemon_reply  *reply
)
{
    if (reply->kind == LOGIN_DAEMON_REPLY_SUCCESS) {
        finish_attempt(conversation, LOGIN_ATTEMPT_SUCCEEDED, NULL);
        return;
    }

    cancel_with_result(conversation, LOGIN_ATTEMPT_FAILED, describe_session_start_failure(reply));
}

/* The attempt is over whatever the daemon replies to the cancel. */
static void
handle_reply_while_cancelling(
    struct login_daemon_conversation *conversation,
    const struct login_daemon_reply  *reply
)
{
    struct login_attempt_details *attempt = &conversation->attempt;

    finish_attempt(conversation, attempt->result_after_cancel, attempt->failure_text_after_cancel);
}

/* NULL in states where no reply is expected. */
static reply_handler
find_reply_handler(
    enum conversation_state state
)
{
    switch (state) {
    case STATE_AUTHENTICATING:
        return handle_reply_while_authenticating;
    case STATE_STARTING_SESSION:
        return handle_reply_while_starting_session;
    case STATE_CANCELLING:
        return handle_reply_while_cancelling;
    default:
        return NULL;
    }
}

static void
handle_decoded_reply(
    struct login_daemon_conversation *conversation,
    const struct login_daemon_reply  *reply
)
{
    reply_handler handle_reply = find_reply_handler(conversation->state);

    if (handle_reply == NULL) {
        fail_attempt_with_message(conversation, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Reply that no request asked for");
        return;
    }

    handle_reply(conversation, reply);
}

/* Fails the attempt and returns false if the payload is not a valid reply. */
static bool
decode_reply_or_fail(
    struct login_daemon_conversation *conversation,
    const char                       *payload,
    size_t                            payload_length,
    struct login_daemon_reply        *reply
)
{
    GError *error = NULL;

    if (login_daemon_decode_reply(reply, payload, payload_length, &error)) {
        return true;
    }

    fail_attempt(conversation, error);
    g_error_free(error);

    return false;
}

static void
handle_received_message(
    void       *user_data,
    uint32_t    message_type,
    const char *payload,
    size_t      payload_length
)
{
    struct login_daemon_conversation *conversation = user_data;
    struct login_daemon_reply         reply;

    if (!decode_reply_or_fail(conversation, payload, payload_length, &reply)) {
        return;
    }

    handle_decoded_reply(conversation, &reply);
    login_daemon_reply_clear(&reply);
}

static void
handle_stream_closed(
    void         *user_data,
    const GError *error
)
{
    struct login_daemon_conversation *conversation = user_data;

    if (error != NULL) {
        fail_attempt(conversation, error);
        return;
    }

    fail_attempt_with_message(conversation, G_IO_ERROR, G_IO_ERROR_CLOSED, "Connection closed by the login daemon");
}

static const struct framed_message_stream_handler stream_handler = {
    .handle_received_message = handle_received_message,
    .handle_stream_closed    = handle_stream_closed,
};

/* Consumes the caller's reference to connection; the stream keeps its own. */
static void
begin_session_creation(
    struct login_daemon_conversation *conversation,
    GSocketConnection                *connection
)
{
    conversation
        ->stream = framed_message_stream_create(connection, &login_daemon_message_format, &stream_handler, conversation);

    conversation->state = STATE_AUTHENTICATING;

    g_object_unref(connection);
    send_request(conversation, login_daemon_encode_create_session_request(conversation->attempt.username));
}

/* Consumes error. A cancelled connection means the conversation may be gone. */
static void
handle_connection_failure(
    struct login_daemon_conversation *conversation,
    GError                           *error
)
{
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        g_error_free(error);
        return;
    }

    fail_attempt(conversation, error);
    g_error_free(error);
}

static void
handle_connected(
    GObject      *source,
    GAsyncResult *result,
    void         *user_data
)
{
    struct login_daemon_conversation *conversation = user_data;
    GError                           *error        = NULL;
    GSocketConnection                *connection   = g_socket_client_connect_finish(G_SOCKET_CLIENT(source), result, &error);

    if (connection == NULL) {
        handle_connection_failure(conversation, error);
        return;
    }

    begin_session_creation(conversation, connection);
}

static void
connect_to_login_daemon(
    struct login_daemon_conversation *conversation
)
{
    GSocketClient  *socket_client = g_socket_client_new();
    GSocketAddress *address       = g_unix_socket_address_new(conversation->socket_path);
    GCancellable   *cancellable   = conversation->connect_cancellable;

    g_socket_client_connect_async(socket_client, G_SOCKET_CONNECTABLE(address), cancellable, handle_connected, conversation);
    g_object_unref(address);
    g_object_unref(socket_client);
}

static void
remember_attempt_details(
    struct login_attempt_details *attempt,
    const char                   *username,
    const char                   *password,
    char *const                  *session_command,
    char *const                  *session_environment
)
{
    attempt->username            = g_strdup(username);
    attempt->password            = g_strdup(password);
    attempt->session_command     = g_strdupv((char **)session_command);
    attempt->session_environment = g_strdupv((char **)session_environment);
}

struct login_daemon_conversation *
login_daemon_conversation_create(
    const char                                     *socket_path,
    const struct login_daemon_conversation_handler *handler,
    void                                           *user_data
)
{
    struct login_daemon_conversation *conversation = g_new0(struct login_daemon_conversation, 1);

    conversation->socket_path = g_strdup(socket_path);
    conversation->handler     = handler;
    conversation->user_data   = user_data;
    conversation->state       = STATE_IDLE;

    return conversation;
}

void
login_daemon_conversation_destroy(
    struct login_daemon_conversation *conversation
)
{
    if (conversation == NULL) {
        return;
    }

    end_attempt(conversation);
    g_free(conversation->socket_path);
    g_free(conversation);
}

bool
login_daemon_conversation_start_attempt(
    struct login_daemon_conversation *conversation,
    const char                       *username,
    const char                       *password,
    char *const                      *session_command,
    char *const                      *session_environment
)
{
    if (conversation->state != STATE_IDLE) {
        return false;
    }

    conversation->state               = STATE_CONNECTING;
    conversation->connect_cancellable = g_cancellable_new();

    remember_attempt_details(&conversation->attempt, username, password, session_command, session_environment);
    connect_to_login_daemon(conversation);

    return true;
}

void
login_daemon_conversation_answer_question(
    struct login_daemon_conversation *conversation,
    const char                       *answer
)
{
    g_return_if_fail(answer != NULL);

    if (conversation->state != STATE_WAITING_ANSWER) {
        return;
    }

    conversation->state = STATE_AUTHENTICATING;

    send_request(conversation, login_daemon_encode_authentication_response(answer));
}

void
login_daemon_conversation_cancel_attempt(
    struct login_daemon_conversation *conversation
)
{
    switch (conversation->state) {
    case STATE_WAITING_ANSWER:
        cancel_with_result(conversation, LOGIN_ATTEMPT_CANCELLED, NULL);

        return;
    case STATE_CONNECTING:
    case STATE_AUTHENTICATING:
        finish_attempt(conversation, LOGIN_ATTEMPT_CANCELLED, NULL);

        return;
    default:
        return;
    }
}
