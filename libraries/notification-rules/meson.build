notification_rules_dependency = declare_dependency(
    link_with: static_library(
        'notification-rules',
        'notification-rules.c',
        dependencies: [notification_request_dependency, program_configuration_file_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [notification_request_dependency, program_configuration_file_dependency],
)

test(
    'notification-rules',
    executable(
        'test-notification-rules',
        'tests/test-notification-rules.c',
        dependencies: [notification_rules_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
