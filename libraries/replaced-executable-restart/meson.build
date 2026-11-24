replaced_executable_restart_dependency = declare_dependency(
    link_with: static_library(
        'replaced-executable-restart',
        'replaced-executable-restart.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: glib_dependency,
)

test(
    'replaced-executable-restart',
    executable(
        'test-replaced-executable-restart',
        'tests/test-replaced-executable-restart.c',
        dependencies: [replaced_executable_restart_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
