filesystem_free_space_dependency = declare_dependency(
    link_with: static_library(
        'filesystem-free-space',
        'filesystem-free-space.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [],
)

test(
    'filesystem-free-space',
    executable(
        'test-filesystem-free-space',
        'tests/test-filesystem-free-space.c',
        dependencies: [filesystem_free_space_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
