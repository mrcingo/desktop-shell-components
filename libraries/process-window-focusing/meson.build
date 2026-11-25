process_window_focusing_dependency = declare_dependency(
    link_with: static_library(
        'process-window-focusing',
        'process-window-focusing.c',
        dependencies: [compositor_ipc_client_dependency, compositor_window_tree_dependency, process_window_lookup_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: compositor_ipc_client_dependency,
)

test(
    'process-window-focusing',
    executable(
        'test-process-window-focusing',
        'tests/test-process-window-focusing.c',
        dependencies: [process_window_focusing_dependency, compositor_ipc_message_format_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
