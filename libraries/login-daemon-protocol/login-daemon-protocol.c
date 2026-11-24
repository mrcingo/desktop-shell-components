#define _DEFAULT_SOURCE // explicit_bzero(), provided by glibc and musl

#include <string.h>

#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "login-daemon-protocol.h"
#include "typed-json-member-lookup.h"

static bool
decode_length_header(
    const uint8_t *header_bytes,
    uint32_t      *payload_length,
    uint32_t      *message_type
)
{
    memcpy(payload_length, header_bytes, sizeof(*payload_length));

    *message_type = 0;

    return true;
}

const struct framed_message_format login_daemon_message_format = {
    .header_size_in_bytes          = LOGIN_DAEMON_HEADER_SIZE,
    .maximum_payload_size_in_bytes = LOGIN_DAEMON_MAXIMUM_PAYLOAD_SIZE,
    .decode_header                 = decode_length_header,
};

/* A frame whose bytes are wiped when its GBytes is released. */
struct wiped_frame {
    size_t  size;
    uint8_t bytes[];
};

static void
wipe_and_free_frame(
    void *data
)
{
    struct wiped_frame *frame = data;

    explicit_bzero(frame->bytes, frame->size);
    g_free(frame);
}

static struct wiped_frame *
create_wiped_frame(
    const char *payload,
    size_t      payload_length
)
{
    uint32_t            length_field = (uint32_t)payload_length;
    size_t              frame_size   = LOGIN_DAEMON_HEADER_SIZE + payload_length;
    struct wiped_frame *frame        = g_malloc(sizeof(*frame) + frame_size);

    frame->size = frame_size;

    memcpy(frame->bytes, &length_field, LOGIN_DAEMON_HEADER_SIZE);
    memcpy(frame->bytes + LOGIN_DAEMON_HEADER_SIZE, payload, payload_length);

    return frame;
}

/* Finishes the builder's object, consumes the builder and returns the JSON text. */
static char *
serialize_and_release_builder(
    JsonBuilder *builder
)
{
    json_builder_end_object(builder);

    JsonNode *root    = json_builder_get_root(builder);
    char     *payload = json_to_string(root, FALSE);

    json_node_unref(root);
    g_object_unref(builder);

    return payload;
}

static size_t
measure_payload(
    const char *payload
)
{
    size_t payload_length = strlen(payload);

    g_assert(payload_length <= LOGIN_DAEMON_MAXIMUM_PAYLOAD_SIZE);

    return payload_length;
}

/*
 * Frames the request and wipes the JSON text. json-glib keeps short-lived
 * internal copies of the values, which cannot be wiped.
 */
static GBytes *
finish_request(
    JsonBuilder *builder
)
{
    char               *payload        = serialize_and_release_builder(builder);
    size_t              payload_length = measure_payload(payload);
    struct wiped_frame *frame          = create_wiped_frame(payload, payload_length);

    explicit_bzero(payload, payload_length);
    g_free(payload);

    return g_bytes_new_with_free_func(frame->bytes, frame->size, wipe_and_free_frame, frame);
}

static JsonBuilder *
begin_request(
    const char *request_type
)
{
    JsonBuilder *builder = json_builder_new();

    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "type");
    json_builder_add_string_value(builder, request_type);

    return builder;
}

GBytes *
login_daemon_encode_create_session_request(
    const char *username
)
{
    JsonBuilder *builder = begin_request("create_session");

    json_builder_set_member_name(builder, "username");
    json_builder_add_string_value(builder, username);

    return finish_request(builder);
}

GBytes *
login_daemon_encode_authentication_response(
    const char *response
)
{
    JsonBuilder *builder = begin_request("post_auth_message_response");

    if (response != NULL) {
        json_builder_set_member_name(builder, "response");
        json_builder_add_string_value(builder, response);
    }

    return finish_request(builder);
}

GBytes *
login_daemon_encode_cancel_session_request(void)
{
    return finish_request(begin_request("cancel_session"));
}

static void
add_shell_quoted_arguments(
    JsonBuilder *builder,
    char *const *command
)
{
    json_builder_begin_array(builder);

    for (size_t i = 0; command[i] != NULL; i++) {
        char *quoted_argument = g_shell_quote(command[i]);

        json_builder_add_string_value(builder, quoted_argument);
        g_free(quoted_argument);
    }

    json_builder_end_array(builder);
}

static void
add_environment(
    JsonBuilder *builder,
    char *const *environment
)
{
    json_builder_begin_array(builder);

    for (size_t i = 0; environment != NULL && environment[i] != NULL; i++) {
        json_builder_add_string_value(builder, environment[i]);
    }

    json_builder_end_array(builder);
}

GBytes *
login_daemon_encode_start_session_request(
    char *const *command,
    char *const *environment
)
{
    JsonBuilder *builder = begin_request("start_session");

    json_builder_set_member_name(builder, "cmd");
    add_shell_quoted_arguments(builder, command);

    json_builder_set_member_name(builder, "env");
    add_environment(builder, environment);

    return finish_request(builder);
}

static bool
fail_as_invalid_reply(
    GError    **error,
    const char *reason
)
{
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Invalid login daemon reply: %s", reason);

    return false;
}

static bool
decode_message_kind(
    const char                                    *kind_name,
    enum login_daemon_authentication_message_kind *message_kind
)
{
    static const char *const kind_names[] = {
        [LOGIN_DAEMON_MESSAGE_VISIBLE_QUESTION] = "visible",
        [LOGIN_DAEMON_MESSAGE_SECRET_QUESTION]  = "secret",
        [LOGIN_DAEMON_MESSAGE_INFORMATION]      = "info",
        [LOGIN_DAEMON_MESSAGE_ERROR]            = "error",
    };

    for (size_t i = 0; i < G_N_ELEMENTS(kind_names); i++) {
        if (g_strcmp0(kind_name, kind_names[i]) == 0) {
            *message_kind = (enum login_daemon_authentication_message_kind)i;
            return true;
        }
    }

    return false;
}

/* The text is never NULL in a decoded reply. */
static void
set_reply_text(
    struct login_daemon_reply *reply,
    const char                *text
)
{
    if (text == NULL) {
        text = "";
    }

    reply->text = g_strstrip(g_strdup(text));
}

static bool
decode_success_reply(
    struct login_daemon_reply *reply,
    JsonObject                *object,
    GError                   **error
)
{
    reply->kind = LOGIN_DAEMON_REPLY_SUCCESS;

    set_reply_text(reply, NULL);

    return true;
}

static bool
decode_error_reply(
    struct login_daemon_reply *reply,
    JsonObject                *object,
    GError                   **error
)
{
    const char *error_type = typed_json_lookup_string_member(object, "error_type");

    reply->kind                      = LOGIN_DAEMON_REPLY_ERROR;
    reply->is_authentication_failure = g_strcmp0(error_type, "auth_error") == 0;

    set_reply_text(reply, typed_json_lookup_string_member(object, "description"));

    return true;
}

static bool
decode_authentication_message_reply(
    struct login_daemon_reply *reply,
    JsonObject                *object,
    GError                   **error
)
{
    const char *kind_name = typed_json_lookup_string_member(object, "auth_message_type");

    reply->kind = LOGIN_DAEMON_REPLY_AUTHENTICATION_MESSAGE;

    set_reply_text(reply, typed_json_lookup_string_member(object, "auth_message"));

    if (!decode_message_kind(kind_name, &reply->message_kind)) {
        return fail_as_invalid_reply(error, "unknown auth_message_type");
    }

    return true;
}

typedef bool (*reply_object_decoder)(struct login_daemon_reply *reply, JsonObject *object, GError **error);

static const struct {
    const char          *reply_type;
    reply_object_decoder decode_reply_object;
} reply_object_decoders[] = {
    {"success",      decode_success_reply               },
    {"error",        decode_error_reply                 },
    {"auth_message", decode_authentication_message_reply},
};

static bool
decode_reply_object(
    struct login_daemon_reply *reply,
    JsonObject                *object,
    GError                   **error
)
{
    const char *reply_type = typed_json_lookup_string_member(object, "type");

    for (size_t i = 0; i < G_N_ELEMENTS(reply_object_decoders); i++) {
        if (g_strcmp0(reply_type, reply_object_decoders[i].reply_type) == 0) {
            return reply_object_decoders[i].decode_reply_object(reply, object, error);
        }
    }

    return fail_as_invalid_reply(error, "unknown type");
}

static bool
decode_parsed_reply(
    struct login_daemon_reply *reply,
    JsonNode                  *root_node,
    GError                   **error
)
{
    JsonObject *object = typed_json_root_object(root_node);

    if (object == NULL) {
        return fail_as_invalid_reply(error, "not an object");
    }

    return decode_reply_object(reply, object, error);
}

static bool
parse_and_decode_reply(
    struct login_daemon_reply *reply,
    const char                *payload,
    size_t                     payload_length,
    GError                   **error
)
{
    JsonParser *parser     = json_parser_new();
    bool        is_parsed  = json_parser_load_from_data(parser, payload, (gssize)payload_length, error);
    bool        is_decoded = is_parsed && decode_parsed_reply(reply, json_parser_get_root(parser), error);

    g_object_unref(parser);

    return is_decoded;
}

/* A failed decode leaves the reply zeroed, with any text it had freed. */
bool
login_daemon_decode_reply(
    struct login_daemon_reply *reply,
    const char                *payload,
    size_t                     payload_length,
    GError                   **error
)
{
    *reply = (struct login_daemon_reply){0};

    if (payload_length > LOGIN_DAEMON_MAXIMUM_PAYLOAD_SIZE) {
        return fail_as_invalid_reply(error, "payload too large");
    }

    bool is_decoded = parse_and_decode_reply(reply, payload, payload_length, error);

    if (!is_decoded) {
        login_daemon_reply_clear(reply);
    }

    return is_decoded;
}

void
login_daemon_reply_clear(
    struct login_daemon_reply *reply
)
{
    g_free(reply->text);

    *reply = (struct login_daemon_reply){0};
}
