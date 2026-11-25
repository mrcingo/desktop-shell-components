human_readable_quantities_dependency = declare_dependency(
    link_with: static_library(
        'human-readable-quantities',
        'human-readable-quantities.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [glib_dependency],
)

test(
    'human-readable-quantities',
    executable(
        'test-human-readable-quantities',
        'tests/test-human-readable-quantities.c',
        dependencies: [human_readable_quantities_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
