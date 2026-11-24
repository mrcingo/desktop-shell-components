login_daemon_conversation_dependency = declare_dependency(
    link_with: static_library(
        'login-daemon-conversation',
        'login-daemon-conversation.c',
        c_args: '-DG_LOG_DOMAIN="login-daemon-conversation"',
        dependencies: [
            framed_message_stream_dependency,
            gio_unix_dependency,
            login_daemon_protocol_dependency,
        ],
    ),
    include_directories: include_directories('.'),
    dependencies: glib_dependency,
)

test(
    'login-daemon-conversation',
    executable(
        'test-login-daemon-conversation',
        'tests/test-login-daemon-conversation.c',
        c_args: '-DG_LOG_DOMAIN="login-daemon-conversation"',
        dependencies: [
            login_daemon_conversation_dependency,
            login_daemon_protocol_dependency,
            gio_dependency,
            json_glib_dependency,
        ],
    ),
    protocol: 'tap',
    env: test_environment,
)
