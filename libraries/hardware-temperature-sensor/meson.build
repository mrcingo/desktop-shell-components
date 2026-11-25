hardware_temperature_sensor_dependency = declare_dependency(
    link_with: static_library(
        'hardware-temperature-sensor',
        'hardware-temperature-sensor.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [],
)

test(
    'hardware-temperature-sensor',
    executable(
        'test-hardware-temperature-sensor',
        'tests/test-hardware-temperature-sensor.c',
        dependencies: [hardware_temperature_sensor_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
