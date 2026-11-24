login_daemon_protocol_dependency = declare_dependency(
    link_with: static_library(
        'login-daemon-protocol',
        'login-daemon-protocol.c',
        dependencies: [
            framed_message_stream_dependency,
            gio_dependency,
            json_glib_dependency,
            typed_json_member_lookup_dependency,
        ],
    ),
    include_directories: include_directories('.'),
    dependencies: [framed_message_stream_dependency, glib_dependency],
)

test(
    'login-daemon-protocol',
    executable(
        'test-login-daemon-protocol',
        'tests/test-login-daemon-protocol.c',
        dependencies: [
            login_daemon_protocol_dependency,
            json_glib_dependency,
        ],
    ),
    protocol: 'tap',
    env: test_environment,
)
