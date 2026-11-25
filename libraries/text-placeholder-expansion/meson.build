text_placeholder_expansion_dependency = declare_dependency(
    link_with: static_library(
        'text-placeholder-expansion',
        'text-placeholder-expansion.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [],
)

test(
    'text-placeholder-expansion',
    executable(
        'test-text-placeholder-expansion',
        'tests/test-text-placeholder-expansion.c',
        dependencies: [text_placeholder_expansion_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
