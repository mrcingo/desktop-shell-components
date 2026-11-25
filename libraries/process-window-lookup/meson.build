process_window_lookup_dependency = declare_dependency(
    link_with: static_library(
        'process-window-lookup',
        'process-window-lookup.c',
        dependencies: [compositor_window_tree_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [compositor_window_tree_dependency],
)

test(
    'process-window-lookup',
    executable(
        'test-process-window-lookup',
        'tests/test-process-window-lookup.c',
        dependencies: [process_window_lookup_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
