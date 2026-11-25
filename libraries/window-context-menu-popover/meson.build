window_context_menu_popover_dependency = declare_dependency(
    link_with: static_library(
        'window-context-menu-popover',
        'window-context-menu-popover.c',
        dependencies: [window_context_menu_actions_dependency, text_ellipsis_truncation_dependency, gtk_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [window_context_menu_target_dependency, compositor_ipc_client_dependency, gtk_dependency],
)
