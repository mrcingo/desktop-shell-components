window_context_menu_actions_dependency = declare_dependency(
    link_with: static_library(
        'window-context-menu-actions',
        'window-context-menu-actions.c',
        dependencies: [window_context_menu_target_dependency, compositor_ipc_client_dependency, gtk_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [window_context_menu_target_dependency, compositor_ipc_client_dependency, gtk_dependency],
)

test(
    'window-context-menu-actions',
    executable(
        'test-window-context-menu-actions',
        'tests/test-window-context-menu-actions.c',
        dependencies: [window_context_menu_actions_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
