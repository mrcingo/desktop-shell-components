rendered_text_width_measurement_dependency = declare_dependency(
    link_with: static_library(
        'rendered-text-width-measurement',
        'rendered-text-width-measurement.c',
        dependencies: [dependency('pangocairo')],
    ),
    include_directories: include_directories('.'),
    dependencies: glib_dependency,
)

test(
    'rendered-text-width-measurement',
    executable(
        'test-rendered-text-width-measurement',
        'tests/test-rendered-text-width-measurement.c',
        dependencies: [rendered_text_width_measurement_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
