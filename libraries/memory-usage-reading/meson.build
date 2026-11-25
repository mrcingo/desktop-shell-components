memory_usage_reading_dependency = declare_dependency(
    link_with: static_library(
        'memory-usage-reading',
        'memory-usage-reading.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [],
)

test(
    'memory-usage-reading',
    executable(
        'test-memory-usage-reading',
        'tests/test-memory-usage-reading.c',
        dependencies: [memory_usage_reading_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
