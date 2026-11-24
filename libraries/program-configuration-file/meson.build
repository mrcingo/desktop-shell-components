program_configuration_file_dependency = declare_dependency(
    link_with: static_library(
        'program-configuration-file',
        'program-configuration-file.c',
        c_args: '-DG_LOG_DOMAIN="program-configuration-file"',
        dependencies: glib_dependency,
    ),
    include_directories: include_directories('.'),
)

test(
    'program-configuration-file',
    executable(
        'test-program-configuration-file',
        'tests/test-program-configuration-file.c',
        c_args: '-DG_LOG_DOMAIN="program-configuration-file"',
        dependencies: [program_configuration_file_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
