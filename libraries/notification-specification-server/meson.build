notification_specification_server_dependency = declare_dependency(
    link_with: static_library(
        'notification-specification-server',
        'notification-specification-server.c',
        dependencies: [notification_request_dependency, gio_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [notification_request_dependency, gio_dependency],
)

test(
    'notification-specification-server',
    executable(
        'test-notification-specification-server',
        'tests/test-notification-specification-server.c',
        dependencies: [notification_specification_server_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
