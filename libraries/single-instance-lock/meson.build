single_instance_lock_dependency = declare_dependency(
    link_with: static_library(
        'single-instance-lock',
        'single-instance-lock.c',
        dependencies: glib_dependency,
    ),
    include_directories: include_directories('.'),
)

test(
    'single-instance-lock',
    executable(
        'test-single-instance-lock',
        'tests/test-single-instance-lock.c',
        dependencies: [single_instance_lock_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
