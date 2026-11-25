battery_charge_status_dependency = declare_dependency(
    link_with: static_library(
        'battery-charge-status',
        'battery-charge-status.c',
        dependencies: [system_bus_properties_dependency, meson.get_compiler('c').find_library('m')],
    ),
    include_directories: include_directories('.'),
    dependencies: [gio_dependency],
)

test(
    'battery-charge-status',
    executable(
        'test-battery-charge-status',
        'tests/test-battery-charge-status.c',
        dependencies: [battery_charge_status_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
