process_information_snapshot_dependency = declare_dependency(
    link_with: static_library(
        'process-information-snapshot',
        'process-information-snapshot.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [glib_dependency],
)

test(
    'process-information-snapshot',
    executable(
        'test-process-information-snapshot',
        'tests/test-process-information-snapshot.c',
        dependencies: [process_information_snapshot_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
