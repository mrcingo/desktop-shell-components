network_manager_status_dependency = declare_dependency(
    link_with: static_library(
        'network-manager-status',
        'network-manager-status.c',
        dependencies: [system_bus_properties_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [gio_dependency],
)

test(
    'network-manager-status',
    executable(
        'test-network-manager-status',
        'tests/test-network-manager-status.c',
        dependencies: [network_manager_status_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
