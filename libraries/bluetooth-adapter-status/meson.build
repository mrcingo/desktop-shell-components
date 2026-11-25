bluetooth_adapter_status_dependency = declare_dependency(
    link_with: static_library(
        'bluetooth-adapter-status',
        'bluetooth-adapter-status.c',
        dependencies: [system_bus_properties_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [gio_dependency],
)

test(
    'bluetooth-adapter-status',
    executable(
        'test-bluetooth-adapter-status',
        'tests/test-bluetooth-adapter-status.c',
        dependencies: [bluetooth_adapter_status_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
