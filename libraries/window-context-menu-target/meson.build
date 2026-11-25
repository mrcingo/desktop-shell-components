window_context_menu_target_dependency = declare_dependency(
    link_with: static_library(
        'window-context-menu-target',
        'window-context-menu-target.c',
        dependencies: [process_information_snapshot_dependency, process_signal_handle_dependency, compositor_window_tree_dependency, glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [process_information_snapshot_dependency, process_signal_handle_dependency, compositor_window_tree_dependency],
)

test(
    'window-context-menu-target',
    executable(
        'test-window-context-menu-target',
        'tests/test-window-context-menu-target.c',
        dependencies: [window_context_menu_target_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
