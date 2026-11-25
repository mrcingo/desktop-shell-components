processor_usage_sampling_dependency = declare_dependency(
    link_with: static_library(
        'processor-usage-sampling',
        'processor-usage-sampling.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [],
)

test(
    'processor-usage-sampling',
    executable(
        'test-processor-usage-sampling',
        'tests/test-processor-usage-sampling.c',
        dependencies: [processor_usage_sampling_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
