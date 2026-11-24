#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>

#include "login-daemon-conversation.h"
#include "login-daemon-protocol.h"

#define SUCCESS "{\"type\":\"success\"}"
#define AUTHENTICATION(kind, text) \
    "{\"type\":\"auth_message\",\"auth_message_type\":\"" kind "\",\"auth_message\":\"" text "\"}"
#define FAILURE(kind, text) "{\"type\":\"error\",\"error_type\":\"" kind "\",\"description\":\"" text "\"}"

/* A reply that keeps the connection open without answering. */
static const char hold[] = "";

/*
 * One step of a scripted login daemon: read a request of the given type,
 * then send the reply. A NULL reply hangs up, hold waits for the client to
 * hang up, and a non-zero length overrides the header.
 */
struct step {
    const char *request;
    const char *reply;
    uint32_t    length;
};

enum step_result {
    STEP_DONE,    // the step ran and the connection stays open
    STEP_CLOSED,  // the step ran and the connection is over
    STEP_SKIPPED, // the client hung up before sending a request
};

struct fake_login_daemon {
    char              *directory;
    char              *socket_path;
    int                listen_fd;
    const struct step *steps;
    GThread           *thread;

    GPtrArray *requests; // char *, JSON payloads
};

/*
 * Records what the conversation reports and plays scripted answers. NULL
 * answers cancel; questions are "secret:" or "visible:" plus their text and
 * messages are "error:" or "info:" plus theirs.
 */
struct test_client {
    struct login_daemon_conversation *conversation;
    GMainLoop                        *loop;
    const char *const                *answers;
    GPtrArray                        *questions;
    GPtrArray                        *messages;
    bool                              is_done;
    enum login_attempt_result         result;
    char                             *failure_text;
};

struct login_scenario {
    struct fake_login_daemon *daemon;
    struct test_client        client;
};

struct test_registration {
    const char *test_path;
    GTestFunc   test_function;
};

static char *session_command[]     = {"compositor", "--debug", NULL};
static char *session_environment[] = {"XDG_CURRENT_DESKTOP=compositor", NULL};

static bool
read_chunk(
    int     fd,
    void   *buffer,
    size_t  length,
    size_t *offset
)
{
    ssize_t bytes_read = read(fd, (char *)buffer + *offset, length - *offset);

    if (bytes_read <= 0) {
        return false;
    }

    *offset += (size_t)bytes_read;

    return true;
}

static bool
read_full(
    int    fd,
    void  *buffer,
    size_t length
)
{
    for (size_t offset = 0; offset < length;) {
        if (!read_chunk(fd, buffer, length, &offset)) {
            return false;
        }
    }

    return true;
}

static void
write_full(
    int         fd,
    const void *buffer,
    size_t      length
)
{
    for (size_t offset = 0; offset < length;) {
        ssize_t bytes_written = write(fd, (const char *)buffer + offset, length - offset);

        g_assert_cmpint(bytes_written, >, 0);

        offset += (size_t)bytes_written;
    }
}

static void
check_request_type(
    const char *payload,
    const char *expected_type
)
{
    JsonParser *parser = json_parser_new();

    g_assert_true(json_parser_load_from_data(parser, payload, -1, NULL));

    JsonObject *request = json_node_get_object(json_parser_get_root(parser));

    g_assert_cmpstr(json_object_get_string_member(request, "type"), ==, expected_type);
    g_object_unref(parser);
}

static void
wait_for_client_hang_up(
    int fd
)
{
    char byte;

    g_assert_false(read_full(fd, &byte, 1));
}

static uint32_t
declared_reply_length(
    const struct step *step,
    uint32_t           reply_length
)
{
    if (step->length != 0) {
        return step->length;
    }

    return reply_length;
}

static void
send_reply(
    int                fd,
    const struct step *step
)
{
    if (step->reply == hold) {
        wait_for_client_hang_up(fd);
        return;
    }

    uint32_t reply_length = (uint32_t)strlen(step->reply);
    uint32_t header       = declared_reply_length(step, reply_length);

    write_full(fd, &header, sizeof(header));
    write_full(fd, step->reply, reply_length);
}

static enum step_result
answer_request(
    int                fd,
    const struct step *step
)
{
    if (step->reply == NULL) {
        return STEP_CLOSED;
    }

    send_reply(fd, step);

    return step->reply == hold ? STEP_CLOSED : STEP_DONE;
}

static bool
read_payload_length(
    int       fd,
    uint32_t *payload_length
)
{
    uint8_t header[LOGIN_DAEMON_HEADER_SIZE];

    if (!read_full(fd, header, sizeof(header))) {
        return false;
    }

    memcpy(payload_length, header, sizeof(*payload_length));

    return true;
}

/* Returns the request's payload, kept in requests, or NULL if the client hung up. */
static char *
receive_request(
    struct fake_login_daemon *daemon,
    int                       fd
)
{
    uint32_t payload_length;

    if (!read_payload_length(fd, &payload_length)) {
        return NULL;
    }

    char *payload = g_malloc0(payload_length + 1);

    g_assert_true(read_full(fd, payload, payload_length));
    g_ptr_array_add(daemon->requests, payload);

    return payload;
}

static enum step_result
serve_step(
    struct fake_login_daemon *daemon,
    int                       fd,
    const struct step        *step
)
{
    char *payload = receive_request(daemon, fd);

    if (payload == NULL) {
        return STEP_SKIPPED;
    }

    check_request_type(payload, step->request);

    return answer_request(fd, step);
}

static int
accept_connection(
    int listen_fd
)
{
    int fd = accept(listen_fd, NULL, NULL);

    if (fd < 0) {
        g_error("accept: %s", g_strerror(errno));
    }

    return fd;
}

static const struct step *
next_step_after(
    const struct step *step,
    enum step_result   result
)
{
    if (result == STEP_SKIPPED) {
        return step;
    }

    return step + 1;
}

/* Serves steps until the connection ends; returns the first step not run. */
static const struct step *
serve_connection(
    struct fake_login_daemon *daemon,
    const struct step        *step
)
{
    int              fd     = accept_connection(daemon->listen_fd);
    enum step_result result = STEP_DONE;

    while (step->request != NULL && result == STEP_DONE) {
        result = serve_step(daemon, fd, step);
        step   = next_step_after(step, result);
    }

    close(fd);

    return step;
}

/* Runs the script, accepting a new connection whenever the client hangs up. */
static void *
serve_script(
    void *data
)
{
    struct fake_login_daemon *daemon = data;
    const struct step        *step   = daemon->steps;

    while (step->request != NULL) {
        step = serve_connection(daemon, step);
    }

    return NULL;
}

static int
listen_on_unix_socket(
    const char *socket_path
)
{
    struct sockaddr_un address   = {.sun_family = AF_UNIX};
    int                listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    g_assert_cmpuint(strlen(socket_path), <, sizeof(address.sun_path));
    strcpy(address.sun_path, socket_path);
    g_assert_cmpint(bind(listen_fd, (struct sockaddr *)&address, sizeof(address)), ==, 0);
    g_assert_cmpint(listen(listen_fd, 1), ==, 0);

    return listen_fd;
}

static struct fake_login_daemon *
fake_login_daemon_create(
    const struct step *steps
)
{
    struct fake_login_daemon *daemon      = g_new0(struct fake_login_daemon, 1);
    char                     *directory   = g_dir_make_tmp("login-daemon-conversation-test-XXXXXX", NULL);
    char                     *socket_path = g_build_filename(directory, "login-daemon.sock", NULL);

    *daemon = (struct fake_login_daemon){
        .directory   = directory,
        .socket_path = socket_path,
        .listen_fd   = listen_on_unix_socket(socket_path),
        .steps       = steps,
        .requests    = g_ptr_array_new_with_free_func(g_free),
    };

    daemon->thread = g_thread_new("fake-login-daemon", serve_script, daemon);

    return daemon;
}

/* Waits until the script has run; requests can be inspected afterwards. */
static void
fake_login_daemon_wait(
    struct fake_login_daemon *daemon
)
{
    if (daemon->thread == NULL) {
        return;
    }

    g_thread_join(daemon->thread);

    daemon->thread = NULL;
}

static void
remove_socket_files(
    struct fake_login_daemon *daemon
)
{
    close(daemon->listen_fd);
    g_unlink(daemon->socket_path);
    g_rmdir(daemon->directory);
}

static void
fake_login_daemon_destroy(
    struct fake_login_daemon *daemon
)
{
    fake_login_daemon_wait(daemon);
    remove_socket_files(daemon);
    g_ptr_array_unref(daemon->requests);
    g_free(daemon->socket_path);
    g_free(daemon->directory);
    g_free(daemon);
}

static JsonObject *
request_at(
    struct fake_login_daemon *daemon,
    guint                     index,
    JsonParser               *parser
)
{
    g_assert_cmpuint(index, <, daemon->requests->len);
    g_assert_true(json_parser_load_from_data(parser, g_ptr_array_index(daemon->requests, index), -1, NULL));

    return json_node_get_object(json_parser_get_root(parser));
}

static void
assert_string_member(
    JsonObject *request,
    const char *member_name,
    const char *expected_value
)
{
    if (expected_value == NULL) {
        g_assert_false(json_object_has_member(request, member_name));
        return;
    }

    g_assert_cmpstr(json_object_get_string_member(request, member_name), ==, expected_value);
}

/* Checks a string member of a request, or its absence when expected_value is NULL. */
static void
assert_request_member(
    struct fake_login_daemon *daemon,
    guint                     index,
    const char               *member_name,
    const char               *expected_value
)
{
    JsonParser *parser  = json_parser_new();
    JsonObject *request = request_at(daemon, index, parser);

    assert_string_member(request, member_name, expected_value);
    g_object_unref(parser);
}

static void
assert_first_array_element(
    JsonObject *request,
    const char *member_name,
    const char *expected_element
)
{
    JsonArray *array = json_object_get_array_member(request, member_name);

    g_assert_cmpstr(json_array_get_string_element(array, 0), ==, expected_element);
}

static void
assert_start_session_request(
    struct fake_login_daemon *daemon,
    guint                     index
)
{
    JsonParser *parser  = json_parser_new();
    JsonObject *request = request_at(daemon, index, parser);

    g_assert_cmpuint(json_array_get_length(json_object_get_array_member(request, "cmd")), ==, 2);
    assert_first_array_element(request, "cmd", "'compositor'");
    assert_first_array_element(request, "env", "XDG_CURRENT_DESKTOP=compositor");
    g_object_unref(parser);
}

static void
answer_or_cancel(
    struct test_client *client,
    const char         *answer
)
{
    if (answer == NULL) {
        login_daemon_conversation_cancel_attempt(client->conversation);
        return;
    }

    login_daemon_conversation_answer_question(client->conversation, answer);
}

static void
record_question(
    void       *user_data,
    const char *question_text,
    bool        answer_is_secret
)
{
    struct test_client *client = user_data;
    const char         *answer = *client->answers++;

    g_ptr_array_add(client->questions, g_strconcat(answer_is_secret ? "secret:" : "visible:", question_text, NULL));
    answer_or_cancel(client, answer);
}

static void
record_message(
    void       *user_data,
    const char *message_text,
    bool        is_error
)
{
    struct test_client *client = user_data;

    g_ptr_array_add(client->messages, g_strconcat(is_error ? "error:" : "info:", message_text, NULL));
}

static void
record_attempt_finished(
    void                     *user_data,
    enum login_attempt_result result,
    const char               *failure_text
)
{
    struct test_client *client = user_data;

    client->is_done      = true;
    client->result       = result;
    client->failure_text = g_strdup(failure_text);

    g_main_loop_quit(client->loop);
}

static const struct login_daemon_conversation_handler recording_handler = {
    .handle_question         = record_question,
    .handle_message          = record_message,
    .handle_attempt_finished = record_attempt_finished,
};

static void
client_init(
    struct test_client *client,
    const char         *socket_path
)
{
    *client = (struct test_client){
        .loop      = g_main_loop_new(NULL, FALSE),
        .questions = g_ptr_array_new_with_free_func(g_free),
        .messages  = g_ptr_array_new_with_free_func(g_free),
    };

    client->conversation = login_daemon_conversation_create(socket_path, &recording_handler, client);
}

/* Starts an attempt; a second start while it runs must be refused. */
static void
client_start(
    struct test_client *client,
    const char         *password,
    const char *const  *answers
)
{
    struct login_daemon_conversation *conversation = client->conversation;

    client->answers = answers;
    client->is_done = false;

    g_clear_pointer(&client->failure_text, g_free);
    g_assert_true(
        login_daemon_conversation_start_attempt(conversation, "monkey", password, session_command, session_environment)
    );
    g_assert_false(
        login_daemon_conversation_start_attempt(conversation, "monkey", password, session_command, session_environment)
    );
}

static void
client_attempt(
    struct test_client *client,
    const char         *password,
    const char *const  *answers
)
{
    client_start(client, password, answers);
    g_main_loop_run(client->loop);
    g_assert_true(client->is_done);
}

static void
client_finish(
    struct test_client *client
)
{
    login_daemon_conversation_destroy(client->conversation);
    g_main_loop_unref(client->loop);
    g_ptr_array_unref(client->questions);
    g_ptr_array_unref(client->messages);
    g_free(client->failure_text);
}

static void
run_main_context(
    gint64 milliseconds
)
{
    gint64 deadline = g_get_monotonic_time() + milliseconds * G_TIME_SPAN_MILLISECOND;

    while (g_get_monotonic_time() < deadline) {
        g_main_context_iteration(NULL, FALSE);
    }
}

static void
prepare_login_scenario(
    struct login_scenario *scenario,
    const struct step     *steps
)
{
    scenario->daemon = fake_login_daemon_create(steps);

    client_init(&scenario->client, scenario->daemon->socket_path);
}

/* Runs one attempt against the script and waits until the script is done. */
static void
run_login_scenario(
    struct login_scenario *scenario,
    const struct step     *steps,
    const char            *password,
    const char *const     *answers
)
{
    prepare_login_scenario(scenario, steps);
    client_attempt(&scenario->client, password, answers);
    fake_login_daemon_wait(scenario->daemon);
}

static void
finish_login_scenario(
    struct login_scenario *scenario
)
{
    client_finish(&scenario->client);
    fake_login_daemon_destroy(scenario->daemon);
}

static void
assert_client_outcome(
    struct test_client       *client,
    enum login_attempt_result expected_result,
    const char               *expected_text
)
{
    g_assert_cmpint(client->result, ==, expected_result);
    g_assert_cmpstr(client->failure_text, ==, expected_text);
}

static void
assert_login_outcome(
    struct login_scenario    *scenario,
    enum login_attempt_result expected_result,
    const char               *expected_text
)
{
    assert_client_outcome(&scenario->client, expected_result, expected_text);
}

static const struct step password_steps[] = {
    {.request = "create_session", .reply = AUTHENTICATION("secret", "Password: ")},
    {.request = "post_auth_message_response", .reply = SUCCESS},
    {.request = "start_session", .reply = SUCCESS},
    {0},
};

static const struct step wrong_password_steps[] = {
    {.request = "create_session", .reply = AUTHENTICATION("secret", "Password:")},
    {.request = "post_auth_message_response", .reply = FAILURE("auth_error", "pam")},
    {.request = "cancel_session", .reply = SUCCESS},
    {0},
};

static const struct step extra_question_steps[] = {
    {.request = "create_session", .reply = AUTHENTICATION("secret", "Password:")},
    {.request = "post_auth_message_response", .reply = AUTHENTICATION("visible", "Code:")},
    {.request = "post_auth_message_response", .reply = SUCCESS},
    {.request = "start_session", .reply = SUCCESS},
    {0},
};

static const struct step new_password_steps[] = {
    {.request = "create_session", .reply = AUTHENTICATION("secret", "Password:")},
    {.request = "post_auth_message_response", .reply = AUTHENTICATION("secret", "New password:")},
    {.request = "post_auth_message_response", .reply = SUCCESS},
    {.request = "start_session", .reply = SUCCESS},
    {0},
};

static const struct step cancelled_question_steps[] = {
    {.request = "create_session", .reply = AUTHENTICATION("secret", "Password:")},
    {.request = "post_auth_message_response", .reply = AUTHENTICATION("secret", "Code:")},
    {.request = "cancel_session", .reply = SUCCESS},
    {0},
};

static const struct step held_steps[] = {
    {.request = "create_session", .reply = hold},
    {0},
};

static const struct step information_steps[] = {
    {.request = "create_session", .reply = AUTHENTICATION("info", "Welcome")},
    {.request = "post_auth_message_response", .reply = AUTHENTICATION("info", "")},
    {.request = "post_auth_message_response", .reply = AUTHENTICATION("secret", "Password:")},
    {.request = "post_auth_message_response", .reply = SUCCESS},
    {.request = "start_session", .reply = SUCCESS},
    {0},
};

static const struct step locked_account_steps[] = {
    {.request = "create_session", .reply = AUTHENTICATION("error", "The account is locked")},
    {.request = "post_auth_message_response", .reply = FAILURE("auth_error", "pam")},
    {.request = "cancel_session", .reply = SUCCESS},
    {0},
};

static const struct step start_error_steps[] = {
    {.request = "create_session", .reply = SUCCESS},
    {.request = "start_session", .reply = FAILURE("error", "no such command")},
    {.request = "cancel_session", .reply = SUCCESS},
    {0},
};

static const struct step question_while_starting_steps[] = {
    {.request = "create_session", .reply = SUCCESS},
    {.request = "start_session", .reply = AUTHENTICATION("secret", "Password:")},
    {.request = "cancel_session", .reply = SUCCESS},
    {0},
};

static const struct step cancel_error_reply_steps[] = {
    {.request = "create_session", .reply = AUTHENTICATION("secret", "Password:")},
    {.request = "post_auth_message_response", .reply = FAILURE("auth_error", "pam")},
    {.request = "cancel_session", .reply = FAILURE("error", "no session")},
    {0},
};

static const struct step retry_steps[] = {
    {.request = "create_session", .reply = AUTHENTICATION("secret", "Password:")},
    {.request = "post_auth_message_response", .reply = FAILURE("auth_error", "pam")},
    {.request = "cancel_session", .reply = SUCCESS},
    {.request = "create_session", .reply = AUTHENTICATION("secret", "Password:")},
    {.request = "post_auth_message_response", .reply = SUCCESS},
    {.request = "start_session", .reply = SUCCESS},
    {0},
};

/* Each of these scripts breaks the protocol after the connection is up. */
static const struct step hang_up_steps[] = {
    {.request = "create_session", .reply = NULL},
    {0},
};

static const struct step malformed_reply_steps[] = {
    {.request = "create_session", .reply = AUTHENTICATION("secret", "Password:")},
    {.request = "post_auth_message_response", .reply = "{oops"},
    {0},
};

static const struct step empty_reply_steps[] = {
    {.request = "create_session", .reply = ""},
    {0},
};

static const struct step oversized_reply_steps[] = {
    {.request = "create_session", .reply = "", .length = LOGIN_DAEMON_MAXIMUM_PAYLOAD_SIZE + 1},
    {0},
};

static const struct step *const broken_scripts[] = {
    hang_up_steps,
    malformed_reply_steps,
    empty_reply_steps,
    oversized_reply_steps,
};

static const char *const one_time_code_answers[] = {"123456"};
static const char *const new_password_answers[]  = {"new"};
static const char *const cancelling_answers[]    = {NULL};

static void
test_password(void)
{
    struct login_scenario scenario;

    run_login_scenario(&scenario, password_steps, "hunter2", NULL);
    assert_login_outcome(&scenario, LOGIN_ATTEMPT_SUCCEEDED, NULL);
    g_assert_cmpuint(scenario.client.questions->len, ==, 0);
    assert_request_member(scenario.daemon, 0, "username", "monkey");
    assert_request_member(scenario.daemon, 1, "response", "hunter2");
    finish_login_scenario(&scenario);
}

static void
test_session_start_request(void)
{
    struct login_scenario scenario;

    run_login_scenario(&scenario, password_steps, "hunter2", NULL);
    assert_start_session_request(scenario.daemon, 2);
    finish_login_scenario(&scenario);
}

static void
test_wrong_password(void)
{
    struct login_scenario scenario;

    run_login_scenario(&scenario, wrong_password_steps, "wrong", NULL);
    assert_login_outcome(&scenario, LOGIN_ATTEMPT_FAILED, "Wrong username or password");
    g_assert_cmpuint(scenario.daemon->requests->len, ==, 3);
    finish_login_scenario(&scenario);
}

static void
test_extra_question(void)
{
    struct login_scenario scenario;

    run_login_scenario(&scenario, extra_question_steps, "hunter2", one_time_code_answers);
    assert_login_outcome(&scenario, LOGIN_ATTEMPT_SUCCEEDED, NULL);
    g_assert_cmpuint(scenario.client.questions->len, ==, 1);
    g_assert_cmpstr(g_ptr_array_index(scenario.client.questions, 0), ==, "visible:Code:");
    assert_request_member(scenario.daemon, 2, "response", "123456");
    finish_login_scenario(&scenario);
}

static void
test_password_not_reused(void)
{
    struct login_scenario scenario;

    run_login_scenario(&scenario, new_password_steps, "old", new_password_answers);
    assert_login_outcome(&scenario, LOGIN_ATTEMPT_SUCCEEDED, NULL);
    g_assert_cmpstr(g_ptr_array_index(scenario.client.questions, 0), ==, "secret:New password:");
    assert_request_member(scenario.daemon, 1, "response", "old");
    assert_request_member(scenario.daemon, 2, "response", "new");
    finish_login_scenario(&scenario);
}

static void
test_cancel_question(void)
{
    struct login_scenario scenario;

    run_login_scenario(&scenario, cancelled_question_steps, "hunter2", cancelling_answers);
    assert_login_outcome(&scenario, LOGIN_ATTEMPT_CANCELLED, NULL);
    finish_login_scenario(&scenario);
}

static gboolean
cancel_soon(
    void *user_data
)
{
    struct test_client *client = user_data;

    login_daemon_conversation_cancel_attempt(client->conversation);

    return G_SOURCE_REMOVE;
}

static void
cancel_after_a_moment(
    struct login_scenario *scenario
)
{
    g_timeout_add(100, cancel_soon, &scenario->client);
    g_main_loop_run(scenario->client.loop);
    fake_login_daemon_wait(scenario->daemon);
}

/* A PAM module that blocks, such as a fingerprint reader, can be abandoned. */
static void
test_cancel_while_waiting(void)
{
    struct login_scenario scenario;

    prepare_login_scenario(&scenario, held_steps);
    client_start(&scenario.client, "hunter2", NULL);
    cancel_after_a_moment(&scenario);
    assert_login_outcome(&scenario, LOGIN_ATTEMPT_CANCELLED, NULL);
    finish_login_scenario(&scenario);
}

static void
test_info_messages(void)
{
    struct login_scenario scenario;

    run_login_scenario(&scenario, information_steps, "hunter2", NULL);
    assert_login_outcome(&scenario, LOGIN_ATTEMPT_SUCCEEDED, NULL);
    g_assert_cmpuint(scenario.client.messages->len, ==, 1);
    g_assert_cmpstr(g_ptr_array_index(scenario.client.messages, 0), ==, "info:Welcome");
    finish_login_scenario(&scenario);
}

/* Information is acknowledged with an empty response; the password answers the question. */
static void
test_info_messages_are_acknowledged(void)
{
    struct login_scenario scenario;

    run_login_scenario(&scenario, information_steps, "hunter2", NULL);
    assert_request_member(scenario.daemon, 1, "response", NULL);
    assert_request_member(scenario.daemon, 2, "response", NULL);
    assert_request_member(scenario.daemon, 3, "response", "hunter2");
    finish_login_scenario(&scenario);
}

static void
test_locked_account(void)
{
    struct login_scenario scenario;

    run_login_scenario(&scenario, locked_account_steps, "hunter2", NULL);
    assert_login_outcome(&scenario, LOGIN_ATTEMPT_FAILED, "The account is locked");
    g_assert_cmpstr(g_ptr_array_index(scenario.client.messages, 0), ==, "error:The account is locked");
    finish_login_scenario(&scenario);
}

static void
test_start_error(void)
{
    struct login_scenario scenario;

    run_login_scenario(&scenario, start_error_steps, "hunter2", NULL);
    assert_login_outcome(&scenario, LOGIN_ATTEMPT_FAILED, "no such command");
    finish_login_scenario(&scenario);
}

static void
test_question_while_starting(void)
{
    struct login_scenario scenario;

    run_login_scenario(&scenario, question_while_starting_steps, "hunter2", NULL);
    assert_login_outcome(&scenario, LOGIN_ATTEMPT_FAILED, "Unexpected question from the login service");
    finish_login_scenario(&scenario);
}

static void
test_cancel_error_reply(void)
{
    struct login_scenario scenario;

    run_login_scenario(&scenario, cancel_error_reply_steps, "wrong", NULL);
    assert_login_outcome(&scenario, LOGIN_ATTEMPT_FAILED, "Wrong username or password");
    finish_login_scenario(&scenario);
}

static void
check_broken_script(
    const struct step *steps
)
{
    struct login_scenario scenario;

    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Login daemon: *");
    run_login_scenario(&scenario, steps, "hunter2", NULL);
    g_test_assert_expected_messages();
    assert_login_outcome(&scenario, LOGIN_ATTEMPT_FAILED, "Login service error");
    finish_login_scenario(&scenario);
}

static void
test_broken_replies(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(broken_scripts); i++) {
        check_broken_script(broken_scripts[i]);
    }
}

static void
test_no_socket(void)
{
    struct test_client client;

    client_init(&client, "/nonexistent/login-daemon.sock");
    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Login daemon: *");
    client_attempt(&client, "hunter2", NULL);
    g_test_assert_expected_messages();
    assert_client_outcome(&client, LOGIN_ATTEMPT_FAILED, "Cannot reach the login service");
    client_finish(&client);
}

/* A failed attempt, then a successful one on the same conversation. */
static void
attempt_wrong_then_right_password(
    struct login_scenario *scenario
)
{
    client_attempt(&scenario->client, "wrong", NULL);
    g_assert_cmpint(scenario->client.result, ==, LOGIN_ATTEMPT_FAILED);
    client_attempt(&scenario->client, "hunter2", NULL);
    fake_login_daemon_wait(scenario->daemon);
}

static void
test_retry(void)
{
    struct login_scenario scenario;

    prepare_login_scenario(&scenario, retry_steps);
    attempt_wrong_then_right_password(&scenario);
    assert_login_outcome(&scenario, LOGIN_ATTEMPT_SUCCEEDED, NULL);
    g_assert_cmpuint(scenario.daemon->requests->len, ==, 6);
    assert_request_member(scenario.daemon, 4, "response", "hunter2");
    finish_login_scenario(&scenario);
}

static void
destroy_conversation_while_pending(
    struct test_client *client
)
{
    run_main_context(100);
    login_daemon_conversation_destroy(client->conversation);

    client->conversation = NULL;

    run_main_context(100);
}

/* Destroying mid-conversation must leave no callback touching freed memory. */
static void
test_destroy_while_pending(void)
{
    struct login_scenario scenario;

    prepare_login_scenario(&scenario, held_steps);
    client_start(&scenario.client, "hunter2", NULL);
    destroy_conversation_while_pending(&scenario.client);
    g_assert_false(scenario.client.is_done);
    finish_login_scenario(&scenario);
}

static const struct test_registration test_registrations[] = {
    {"/login-daemon-conversation/password",                   test_password                      },
    {"/login-daemon-conversation/session-start-request",      test_session_start_request         },
    {"/login-daemon-conversation/wrong-password",             test_wrong_password                },
    {"/login-daemon-conversation/extra-question",             test_extra_question                },
    {"/login-daemon-conversation/password-not-reused",        test_password_not_reused           },
    {"/login-daemon-conversation/cancel-question",            test_cancel_question               },
    {"/login-daemon-conversation/cancel-while-waiting",       test_cancel_while_waiting          },
    {"/login-daemon-conversation/info-messages",              test_info_messages                 },
    {"/login-daemon-conversation/info-messages-acknowledged", test_info_messages_are_acknowledged},
    {"/login-daemon-conversation/locked-account",             test_locked_account                },
    {"/login-daemon-conversation/start-error",                test_start_error                   },
    {"/login-daemon-conversation/question-while-starting",    test_question_while_starting       },
    {"/login-daemon-conversation/cancel-error-reply",         test_cancel_error_reply            },
    {"/login-daemon-conversation/broken-replies",             test_broken_replies                },
    {"/login-daemon-conversation/no-socket",                  test_no_socket                     },
    {"/login-daemon-conversation/retry",                      test_retry                         },
    {"/login-daemon-conversation/destroy-while-pending",      test_destroy_while_pending         },
};

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);

    for (size_t i = 0; i < G_N_ELEMENTS(test_registrations); i++) {
        g_test_add_func(test_registrations[i].test_path, test_registrations[i].test_function);
    }

    return g_test_run();
}
