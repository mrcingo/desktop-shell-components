window_snap_keyboard_steps_dependency = declare_dependency(
    link_with: static_library(
        'window-snap-keyboard-steps',
        'window-snap-keyboard-steps.c',
        dependencies: [compositor_window_tree_dependency, glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: compositor_window_tree_dependency,
)

test(
    'window-snap-keyboard-steps',
    executable(
        'test-window-snap-keyboard-steps',
        'tests/test-window-snap-keyboard-steps.c',
        dependencies: [window_snap_keyboard_steps_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
