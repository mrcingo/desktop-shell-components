compositor_configuration_directives_dependency = declare_dependency(
    link_with: static_library(
        'compositor-configuration-directives',
        'compositor-configuration-directives.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: glib_dependency,
)

test(
    'compositor-configuration-directives',
    executable(
        'test-compositor-configuration-directives',
        'tests/test-compositor-configuration-directives.c',
        dependencies: [compositor_configuration_directives_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
