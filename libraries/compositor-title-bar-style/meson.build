compositor_title_bar_style_dependency = declare_dependency(
    link_with: static_library(
        'compositor-title-bar-style',
        'compositor-title-bar-style.c',
        dependencies: [compositor_configuration_directives_dependency, glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: glib_dependency,
)

test(
    'compositor-title-bar-style',
    executable(
        'test-compositor-title-bar-style',
        'tests/test-compositor-title-bar-style.c',
        dependencies: [compositor_title_bar_style_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
