compositor_ipc_client_dependency = declare_dependency(
    link_with: static_library(
        'compositor-ipc-client',
        'compositor-ipc-client.c',
        dependencies: [
            compositor_ipc_message_format_dependency,
            gio_unix_dependency,
            json_glib_dependency,
        ],
    ),
    include_directories: include_directories('.'),
    dependencies: [
        compositor_ipc_message_format_dependency,
        gio_dependency,
        json_glib_dependency,
    ],
)

test(
    'compositor-ipc-client',
    executable(
        'test-compositor-ipc-client',
        'tests/test-compositor-ipc-client.c',
        dependencies: compositor_ipc_client_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
