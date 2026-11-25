managed_file_writing_dependency = declare_dependency(
    link_with: static_library(
        'managed-file-writing',
        'managed-file-writing.c',
        dependencies: gio_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: glib_dependency,
)

test(
    'managed-file-writing',
    executable('test-managed-file-writing', 'tests/test-managed-file-writing.c', dependencies: managed_file_writing_dependency),
    protocol: 'tap',
    env: test_environment,
)
