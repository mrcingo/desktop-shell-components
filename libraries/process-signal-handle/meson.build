process_signal_handle_dependency = declare_dependency(
    link_with: static_library(
        'process-signal-handle',
        'process-signal-handle.c',
        dependencies: [gio_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [glib_dependency],
)

test(
    'process-signal-handle',
    executable(
        'test-process-signal-handle',
        'tests/test-process-signal-handle.c',
        dependencies: [process_signal_handle_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
