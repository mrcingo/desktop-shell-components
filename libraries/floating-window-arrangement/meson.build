floating_window_arrangement_dependency = declare_dependency(
    link_with: static_library(
        'floating-window-arrangement',
        'floating-window-arrangement.c',
        dependencies: [compositor_window_tree_dependency, glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [compositor_window_tree_dependency, glib_dependency],
)

test(
    'floating-window-arrangement',
    executable(
        'test-floating-window-arrangement',
        'tests/test-floating-window-arrangement.c',
        dependencies: floating_window_arrangement_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
