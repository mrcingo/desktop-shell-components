system_service_readings_dependency = declare_dependency(
    link_with: static_library(
        'system-service-readings',
        'system-service-readings.c',
        dependencies: [system_bus_properties_dependency, network_manager_status_dependency, bluetooth_adapter_status_dependency, power_profile_status_dependency, battery_charge_status_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [network_manager_status_dependency, bluetooth_adapter_status_dependency, power_profile_status_dependency, battery_charge_status_dependency],
)

test(
    'system-service-readings',
    executable(
        'test-system-service-readings',
        'tests/test-system-service-readings.c',
        dependencies: [system_service_readings_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
