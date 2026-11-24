#include <string.h>

#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "login-daemon-protocol.h"

struct test_registration {
    const char *test_path;
    GTestFunc   test_function;
};

/* A framed request, checked and parsed back into JSON. */
struct decoded_request {
    JsonParser *parser;
    JsonObject *object;
};

static char *session_command[]     = {"sway", "--unsupported-gpu", NULL};
static char *session_environment[] = {"XDG_CURRENT_DESKTOP=sway", NULL};

/* Arguments that a shell would split, expand or glob unless quoted. */
static char *shell_sensitive_command[] = {"printf", "%s\\n", "a b", "it's", "$HOME", "x;y", "*", "", NULL};

static void
check_frame_header(
    const uint8_t *frame_bytes,
    size_t         frame_size
)
{
    uint32_t payload_length = 0;
    uint32_t message_type   = 0;

    g_assert_cmpuint(frame_size, >=, LOGIN_DAEMON_HEADER_SIZE);
    g_assert_true(login_daemon_message_format.decode_header(frame_bytes, &payload_length, &message_type));
    g_assert_cmpuint(payload_length, ==, frame_size - LOGIN_DAEMON_HEADER_SIZE);
}

static void
load_request_payload(
    JsonParser    *parser,
    const uint8_t *frame_bytes,
    size_t         frame_size
)
{
    const char *payload        = (const char *)frame_bytes + LOGIN_DAEMON_HEADER_SIZE;
    gssize      payload_length = (gssize)(frame_size - LOGIN_DAEMON_HEADER_SIZE);

    check_frame_header(frame_bytes, frame_size);
    g_assert_true(json_parser_load_from_data(parser, payload, payload_length, NULL));
}

/* Consumes request. */
static struct decoded_request
decode_request(
    GBytes *request
)
{
    size_t                 frame_size;
    const uint8_t         *frame_bytes = g_bytes_get_data(request, &frame_size);
    struct decoded_request decoded     = {.parser = json_parser_new()};

    load_request_payload(decoded.parser, frame_bytes, frame_size);

    decoded.object = json_node_get_object(json_parser_get_root(decoded.parser));

    g_bytes_unref(request);

    return decoded;
}

static void
release_decoded_request(
    struct decoded_request *decoded
)
{
    g_object_unref(decoded->parser);
}

static const char *
request_member(
    struct decoded_request *decoded,
    const char             *member_name
)
{
    return json_object_get_string_member(decoded->object, member_name);
}

static guint
request_array_length(
    struct decoded_request *decoded,
    const char             *member_name
)
{
    return json_array_get_length(json_object_get_array_member(decoded->object, member_name));
}

static void
test_create_session(void)
{
    struct decoded_request decoded = decode_request(login_daemon_encode_create_session_request("mon\"key\n"));

    g_assert_cmpstr(request_member(&decoded, "type"), ==, "create_session");
    g_assert_cmpstr(request_member(&decoded, "username"), ==, "mon\"key\n");
    release_decoded_request(&decoded);
}

static void
test_post_response(void)
{
    struct decoded_request decoded = decode_request(login_daemon_encode_authentication_response("p\\w"));

    g_assert_cmpstr(request_member(&decoded, "type"), ==, "post_auth_message_response");
    g_assert_cmpstr(request_member(&decoded, "response"), ==, "p\\w");
    release_decoded_request(&decoded);
}

static void
test_post_empty_response(void)
{
    struct decoded_request decoded = decode_request(login_daemon_encode_authentication_response(NULL));

    g_assert_cmpstr(request_member(&decoded, "type"), ==, "post_auth_message_response");
    g_assert_false(json_object_has_member(decoded.object, "response"));
    release_decoded_request(&decoded);
}

static void
test_start_session(void)
{
    struct decoded_request decoded = decode_request(
        login_daemon_encode_start_session_request(session_command, session_environment)
    );
    JsonArray *arguments = json_object_get_array_member(decoded.object, "cmd");

    g_assert_cmpuint(json_array_get_length(arguments), ==, 2);
    g_assert_cmpstr(json_array_get_string_element(arguments, 1), ==, "'--unsupported-gpu'");
    g_assert_cmpuint(request_array_length(&decoded, "env"), ==, 1);
    release_decoded_request(&decoded);
}

static void
test_start_session_without_environment(void)
{
    struct decoded_request decoded = decode_request(login_daemon_encode_start_session_request(session_command, NULL));

    g_assert_cmpuint(request_array_length(&decoded, "env"), ==, 0);
    release_decoded_request(&decoded);
}

/* The daemon joins the command with spaces, as a shell line. */
static char *
join_command_line(
    JsonArray *quoted_arguments
)
{
    GString *command_line = g_string_new(NULL);

    for (guint i = 0; i < json_array_get_length(quoted_arguments); i++) {
        g_string_append_printf(command_line, "%s%s", i > 0 ? " " : "", json_array_get_string_element(quoted_arguments, i));
    }

    return g_string_free(command_line, FALSE);
}

static char *
run_through_shell(
    char *command_line
)
{
    char *shell_arguments[] = {"/bin/sh", "-c", command_line, NULL};
    char *shell_output      = NULL;

    g_assert_true(g_spawn_sync(NULL, shell_arguments, NULL, G_SPAWN_DEFAULT, NULL, NULL, &shell_output, NULL, NULL, NULL));
    g_free(command_line);

    return shell_output;
}

/*
 * greetd runs cmd joined with spaces through sh -c. Every argument must come
 * back from a real shell exactly as it went in.
 */
static void
test_start_session_quoting(void)
{
    struct decoded_request decoded = decode_request(login_daemon_encode_start_session_request(shell_sensitive_command, NULL));
    char                  *command_line = join_command_line(json_object_get_array_member(decoded.object, "cmd"));
    char                  *shell_output = run_through_shell(command_line);

    g_assert_cmpstr(shell_output, ==, "a b\nit's\n$HOME\nx;y\n*\n\n");
    g_free(shell_output);
    release_decoded_request(&decoded);
}

static void
test_cancel_session(void)
{
    struct decoded_request decoded = decode_request(login_daemon_encode_cancel_session_request());

    g_assert_cmpstr(request_member(&decoded, "type"), ==, "cancel_session");
    release_decoded_request(&decoded);
}

static bool
decode_reply_text(
    struct login_daemon_reply *reply,
    const char                *json_text
)
{
    GError *error      = NULL;
    bool    is_decoded = login_daemon_decode_reply(reply, json_text, strlen(json_text), &error);

    g_assert_true(is_decoded == (error == NULL));
    g_clear_error(&error);

    return is_decoded;
}

static void
test_reply_success(void)
{
    struct login_daemon_reply reply;

    g_assert_true(decode_reply_text(&reply, "{\"type\":\"success\"}"));
    g_assert_cmpint(reply.kind, ==, LOGIN_DAEMON_REPLY_SUCCESS);
    g_assert_cmpstr(reply.text, ==, "");
    login_daemon_reply_clear(&reply);
}

static void
test_reply_authentication_error(void)
{
    struct login_daemon_reply reply;

    g_assert_true(decode_reply_text(&reply, "{\"type\":\"error\",\"error_type\":\"auth_error\",\"description\":\"pam\"}"));
    g_assert_cmpint(reply.kind, ==, LOGIN_DAEMON_REPLY_ERROR);
    g_assert_true(reply.is_authentication_failure);
    g_assert_cmpstr(reply.text, ==, "pam");
    login_daemon_reply_clear(&reply);
}

static void
test_reply_other_error(void)
{
    struct login_daemon_reply reply;

    g_assert_true(decode_reply_text(&reply, "{\"type\":\"error\",\"error_type\":\"error\",\"description\":\"broken\"}"));
    g_assert_false(reply.is_authentication_failure);
    login_daemon_reply_clear(&reply);
}

static const struct {
    const char                                   *kind_name;
    enum login_daemon_authentication_message_kind message_kind;
} authentication_message_cases[] = {
    {"visible", LOGIN_DAEMON_MESSAGE_VISIBLE_QUESTION},
    {"secret",  LOGIN_DAEMON_MESSAGE_SECRET_QUESTION },
    {"info",    LOGIN_DAEMON_MESSAGE_INFORMATION     },
    {"error",   LOGIN_DAEMON_MESSAGE_ERROR           },
};

static char *
format_authentication_message_reply(
    const char *kind_name
)
{
    const char *json_format = "{\"type\":\"auth_message\",\"auth_message_type\":\"%s\",\"auth_message\":\" Password: \"}";

    return g_strdup_printf(json_format, kind_name);
}

static void
check_authentication_message_kind(
    size_t case_index
)
{
    char *json_text = format_authentication_message_reply(authentication_message_cases[case_index].kind_name);
    struct login_daemon_reply reply;

    g_assert_true(decode_reply_text(&reply, json_text));
    g_assert_cmpint(reply.message_kind, ==, authentication_message_cases[case_index].message_kind);
    g_assert_cmpstr(reply.text, ==, "Password:");
    login_daemon_reply_clear(&reply);
    g_free(json_text);
}

static void
test_reply_auth_message(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(authentication_message_cases); i++) {
        check_authentication_message_kind(i);
    }
}

static const char *const invalid_replies[] = {
    "",
    "{oops",
    "[]",
    "{}",
    "{\"type\":3}",
    "{\"type\":\"telepathy\"}",
    "{\"type\":\"auth_message\",\"auth_message_type\":\"mind\"}",
};

static void
test_reply_invalid(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(invalid_replies); i++) {
        struct login_daemon_reply reply;

        g_assert_false(decode_reply_text(&reply, invalid_replies[i]));
        g_assert_null(reply.text);
    }
}

static void
test_reply_too_large(void)
{
    struct login_daemon_reply reply;
    GError                   *error = NULL;

    g_assert_false(login_daemon_decode_reply(&reply, "{}", (size_t)LOGIN_DAEMON_MAXIMUM_PAYLOAD_SIZE + 1, &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
    g_error_free(error);
}

static void
test_reply_non_string_description(void)
{
    struct login_daemon_reply reply;

    g_assert_true(decode_reply_text(&reply, "{\"type\":\"error\",\"description\":7}"));
    g_assert_cmpstr(reply.text, ==, "");
    login_daemon_reply_clear(&reply);
}

static const struct test_registration test_registrations[] = {
    {"/login-daemon-protocol/create-session",               test_create_session                   },
    {"/login-daemon-protocol/post-response",                test_post_response                    },
    {"/login-daemon-protocol/post-empty-response",          test_post_empty_response              },
    {"/login-daemon-protocol/start-session",                test_start_session                    },
    {"/login-daemon-protocol/start-session/no-environment", test_start_session_without_environment},
    {"/login-daemon-protocol/start-session/quoting",        test_start_session_quoting            },
    {"/login-daemon-protocol/cancel-session",               test_cancel_session                   },
    {"/login-daemon-protocol/reply/success",                test_reply_success                    },
    {"/login-daemon-protocol/reply/authentication-error",   test_reply_authentication_error       },
    {"/login-daemon-protocol/reply/other-error",            test_reply_other_error                },
    {"/login-daemon-protocol/reply/auth-message",           test_reply_auth_message               },
    {"/login-daemon-protocol/reply/invalid",                test_reply_invalid                    },
    {"/login-daemon-protocol/reply/too-large",              test_reply_too_large                  },
    {"/login-daemon-protocol/reply/non-string-description", test_reply_non_string_description     },
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
