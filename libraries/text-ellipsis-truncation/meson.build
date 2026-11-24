text_ellipsis_truncation_dependency = declare_dependency(
    link_with: static_library(
        'text-ellipsis-truncation',
        'text-ellipsis-truncation.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: glib_dependency,
)

test(
    'text-ellipsis-truncation',
    executable(
        'test-text-ellipsis-truncation',
        'tests/test-text-ellipsis-truncation.c',
        dependencies: [text_ellipsis_truncation_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
