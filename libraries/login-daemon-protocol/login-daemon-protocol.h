#ifndef LOGIN_DAEMON_PROTOCOL_H
#define LOGIN_DAEMON_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <glib.h>

#include "framed-message-stream.h"

/*
 * The JSON protocol of the greetd login daemon, see greetd-ipc(7): every
 * message is a 32-bit native-endian payload length followed by a UTF-8 JSON
 * payload. Requests are encoded here; replies are decoded here.
 */

enum {
    LOGIN_DAEMON_HEADER_SIZE          = sizeof(uint32_t),
    LOGIN_DAEMON_MAXIMUM_PAYLOAD_SIZE = 1 << 20,
};

enum login_daemon_reply_kind {
    LOGIN_DAEMON_REPLY_SUCCESS,
    LOGIN_DAEMON_REPLY_ERROR,
    LOGIN_DAEMON_REPLY_AUTHENTICATION_MESSAGE,
};

enum login_daemon_authentication_message_kind {
    LOGIN_DAEMON_MESSAGE_VISIBLE_QUESTION,
    LOGIN_DAEMON_MESSAGE_SECRET_QUESTION,
    LOGIN_DAEMON_MESSAGE_INFORMATION,
    LOGIN_DAEMON_MESSAGE_ERROR,
};

struct login_daemon_reply {
    enum login_daemon_reply_kind kind;

    enum login_daemon_authentication_message_kind message_kind;              // messages only
    bool                                          is_authentication_failure; // errors only
    char                                         *text;                      // description or message, never NULL once decoded
};

extern const struct framed_message_format login_daemon_message_format;

/*
 * Requests, framed and ready to write. Their memory is wiped when released,
 * as an authentication response may hold a password.
 */
GBytes *login_daemon_encode_create_session_request(const char *username);
GBytes *login_daemon_encode_authentication_response(const char *response);
GBytes *login_daemon_encode_cancel_session_request(void);

/*
 * The daemon joins command with spaces and runs the result with sh -c, so
 * each argument is shell-quoted to arrive unchanged. environment may be
 * NULL.
 */
GBytes *login_daemon_encode_start_session_request(char *const *command, char *const *environment);

/*
 * Decodes a reply payload. On failure the reply is left zeroed and error is
 * set. A decoded reply must be released with login_daemon_reply_clear().
 */
bool login_daemon_decode_reply(struct login_daemon_reply *reply, const char *payload, size_t payload_length, GError **error);
void login_daemon_reply_clear(struct login_daemon_reply *reply);

#endif
