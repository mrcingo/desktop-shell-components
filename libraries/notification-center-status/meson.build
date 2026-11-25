notification_center_status_dependency = declare_dependency(
    link_with: static_library(
        'notification-center-status',
        'notification-center-status.c',
        dependencies: [system_bus_properties_dependency, gio_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [gio_dependency],
)

test(
    'notification-center-status',
    executable(
        'test-notification-center-status',
        'tests/test-notification-center-status.c',
        dependencies: [notification_center_status_dependency, gio_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
