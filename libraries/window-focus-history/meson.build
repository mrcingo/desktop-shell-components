window_focus_history_dependency = declare_dependency(
    link_with: static_library(
        'window-focus-history',
        'window-focus-history.c',
        dependencies: glib_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: glib_dependency,
)

test(
    'window-focus-history',
    executable(
        'test-window-focus-history',
        'tests/test-window-focus-history.c',
        dependencies: window_focus_history_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
