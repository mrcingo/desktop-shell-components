notification_store_dependency = declare_dependency(
    link_with: static_library(
        'notification-store',
        'notification-store.c',
        dependencies: [notification_request_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [notification_request_dependency],
)

test(
    'notification-store',
    executable(
        'test-notification-store',
        'tests/test-notification-store.c',
        dependencies: [notification_store_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
