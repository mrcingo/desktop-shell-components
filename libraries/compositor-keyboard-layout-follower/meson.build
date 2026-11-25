compositor_keyboard_layout_follower_dependency = declare_dependency(
    link_with: static_library(
        'compositor-keyboard-layout-follower',
        'compositor-keyboard-layout-follower.c',
        dependencies: [compositor_ipc_client_dependency, compositor_keyboard_layout_dependency, keyboard_layout_short_names_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [compositor_ipc_client_dependency],
)

test(
    'compositor-keyboard-layout-follower',
    executable(
        'test-compositor-keyboard-layout-follower',
        'tests/test-compositor-keyboard-layout-follower.c',
        dependencies: [compositor_keyboard_layout_follower_dependency, compositor_ipc_message_format_dependency, gio_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
