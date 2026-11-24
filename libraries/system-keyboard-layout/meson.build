system_keyboard_layout_dependency = declare_dependency(
    link_with: static_library(
        'system-keyboard-layout',
        'system-keyboard-layout.c',
        dependencies: gio_dependency,
    ),
    include_directories: include_directories('.'),
)

test(
    'system-keyboard-layout',
    executable(
        'test-system-keyboard-layout',
        'tests/test-system-keyboard-layout.c',
        dependencies: [system_keyboard_layout_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
