clock_boundary_timing_dependency = declare_dependency(
    link_with: static_library(
        'clock-boundary-timing',
        'clock-boundary-timing.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [glib_dependency],
)

test(
    'clock-boundary-timing',
    executable(
        'test-clock-boundary-timing',
        'tests/test-clock-boundary-timing.c',
        dependencies: [clock_boundary_timing_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
