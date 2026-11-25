notification_request_dependency = declare_dependency(
    link_with: static_library(
        'notification-request',
        'notification-request.c',
        dependencies: [text_ellipsis_truncation_dependency, glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [glib_dependency],
)

test(
    'notification-request',
    executable(
        'test-notification-request',
        'tests/test-notification-request.c',
        dependencies: [notification_request_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
