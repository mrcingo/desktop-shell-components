compositor_ipc_message_format_dependency = declare_dependency(
    link_with: static_library(
        'compositor-ipc-message-format',
        'compositor-ipc-message-format.c',
        dependencies: framed_message_stream_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: framed_message_stream_dependency,
)

test(
    'compositor-ipc-message-format',
    executable(
        'test-compositor-ipc-message-format',
        'tests/test-compositor-ipc-message-format.c',
        dependencies: compositor_ipc_message_format_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
