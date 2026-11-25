compositor_keyboard_layout_dependency = declare_dependency(
    link_with: static_library(
        'compositor-keyboard-layout',
        'compositor-keyboard-layout.c',
        dependencies: [typed_json_member_lookup_dependency, json_glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [json_glib_dependency],
)

test(
    'compositor-keyboard-layout',
    executable(
        'test-compositor-keyboard-layout',
        'tests/test-compositor-keyboard-layout.c',
        dependencies: [compositor_keyboard_layout_dependency, json_glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
