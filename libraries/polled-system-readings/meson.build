polled_system_readings_dependency = declare_dependency(
    link_with: static_library(
        'polled-system-readings',
        'polled-system-readings.c',
        dependencies: [processor_usage_sampling_dependency, hardware_temperature_sensor_dependency, memory_usage_reading_dependency, filesystem_free_space_dependency, network_interface_traffic_dependency, display_backlight_level_dependency, glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [memory_usage_reading_dependency, filesystem_free_space_dependency, glib_dependency],
)

test(
    'polled-system-readings',
    executable(
        'test-polled-system-readings',
        'tests/test-polled-system-readings.c',
        dependencies: [polled_system_readings_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
