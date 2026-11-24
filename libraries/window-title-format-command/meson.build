window_title_format_command_dependency = declare_dependency(
    link_with: static_library(
        'window-title-format-command',
        'window-title-format-command.c',
        dependencies: [compositor_window_tree_dependency, window_label_format_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: compositor_window_tree_dependency,
)

test(
    'window-title-format-command',
    executable(
        'test-window-title-format-command',
        'tests/test-window-title-format-command.c',
        dependencies: [window_title_format_command_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
