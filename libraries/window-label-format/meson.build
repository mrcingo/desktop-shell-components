window_label_format_dependency = declare_dependency(
    link_with: static_library(
        'window-label-format',
        'window-label-format.c',
        dependencies: glib_dependency,
    ),
    include_directories: include_directories('.'),
)

test(
    'window-label-format',
    executable(
        'test-window-label-format',
        'tests/test-window-label-format.c',
        dependencies: [window_label_format_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
