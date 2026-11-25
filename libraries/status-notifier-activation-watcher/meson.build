status_notifier_activation_watcher_dependency = declare_dependency(
    link_with: static_library(
        'status-notifier-activation-watcher',
        'status-notifier-activation-watcher.c',
        dependencies: gio_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: gio_dependency,
)

test(
    'status-notifier-activation-watcher',
    executable(
        'test-status-notifier-activation-watcher',
        'tests/test-status-notifier-activation-watcher.c',
        dependencies: status_notifier_activation_watcher_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
