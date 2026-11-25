system_bus_properties_dependency = declare_dependency(
    link_with: static_library(
        'system-bus-properties',
        'system-bus-properties.c',
        dependencies: [gio_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [gio_dependency],
)

test(
    'system-bus-properties',
    executable(
        'test-system-bus-properties',
        'tests/test-system-bus-properties.c',
        dependencies: [system_bus_properties_dependency, gio_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
