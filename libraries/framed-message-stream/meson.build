framed_message_stream_dependency = declare_dependency(
    link_with: static_library(
        'framed-message-stream',
        'framed-message-stream.c',
        dependencies: gio_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: gio_dependency,
)

test(
    'framed-message-stream',
    executable(
        'test-framed-message-stream',
        'tests/test-framed-message-stream.c',
        dependencies: [framed_message_stream_dependency, gio_unix_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
