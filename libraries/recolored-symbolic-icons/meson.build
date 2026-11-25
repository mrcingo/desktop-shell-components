recolored_symbolic_icons_dependency = declare_dependency(
    link_with: static_library(
        'recolored-symbolic-icons',
        'recolored-symbolic-icons.c',
        dependencies: [managed_file_writing_dependency, glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: glib_dependency,
)

test(
    'recolored-symbolic-icons',
    executable(
        'test-recolored-symbolic-icons',
        'tests/test-recolored-symbolic-icons.c',
        dependencies: recolored_symbolic_icons_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
