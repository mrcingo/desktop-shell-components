power_profile_status_dependency = declare_dependency(
    link_with: static_library(
        'power-profile-status',
        'power-profile-status.c',
        dependencies: [system_bus_properties_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [gio_dependency],
)

test(
    'power-profile-status',
    executable(
        'test-power-profile-status',
        'tests/test-power-profile-status.c',
        dependencies: [power_profile_status_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
