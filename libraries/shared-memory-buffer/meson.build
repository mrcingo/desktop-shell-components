shared_memory_buffer_dependency = declare_dependency(
    link_with: static_library(
        'shared-memory-buffer',
        'shared-memory-buffer.c',
        dependencies: glib_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: glib_dependency,
)

test(
    'shared-memory-buffer',
    executable(
        'test-shared-memory-buffer',
        'tests/test-shared-memory-buffer.c',
        dependencies: shared_memory_buffer_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
