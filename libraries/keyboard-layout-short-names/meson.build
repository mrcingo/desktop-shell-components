keyboard_layout_short_names_dependency = declare_dependency(
    link_with: static_library(
        'keyboard-layout-short-names',
        'keyboard-layout-short-names.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [],
)

test(
    'keyboard-layout-short-names',
    executable(
        'test-keyboard-layout-short-names',
        'tests/test-keyboard-layout-short-names.c',
        dependencies: [keyboard_layout_short_names_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
