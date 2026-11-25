exclusive_file_writing_dependency = declare_dependency(
    link_with: static_library(
        'exclusive-file-writing',
        'exclusive-file-writing.c',
        dependencies: gio_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: gio_dependency,
)

test(
    'exclusive-file-writing',
    executable(
        'test-exclusive-file-writing',
        'tests/test-exclusive-file-writing.c',
        dependencies: exclusive_file_writing_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
