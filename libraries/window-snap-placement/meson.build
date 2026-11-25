window_snap_placement_dependency = declare_dependency(
    link_with: static_library(
        'window-snap-placement',
        'window-snap-placement.c',
        dependencies: [compositor_window_tree_dependency, glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: compositor_window_tree_dependency,
)

test(
    'window-snap-placement',
    executable(
        'test-window-snap-placement',
        'tests/test-window-snap-placement.c',
        dependencies: [window_snap_placement_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
