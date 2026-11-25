theme_palette_dependency = declare_dependency(
    link_with: static_library(
        'theme-palette',
        'theme-palette.c',
        dependencies: [glib_dependency, meson.get_compiler('c').find_library('m')],
    ),
    include_directories: include_directories('.'),
    dependencies: [glib_dependency, meson.get_compiler('c').find_library('m')],
)

test(
    'theme-palette',
    executable('test-theme-palette', 'tests/test-theme-palette.c', dependencies: theme_palette_dependency),
    protocol: 'tap',
    env: test_environment,
)
