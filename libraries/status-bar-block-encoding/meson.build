status_bar_block_encoding_dependency = declare_dependency(
    link_with: static_library(
        'status-bar-block-encoding',
        'status-bar-block-encoding.c',
        dependencies: [json_glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [glib_dependency],
)

test(
    'status-bar-block-encoding',
    executable(
        'test-status-bar-block-encoding',
        'tests/test-status-bar-block-encoding.c',
        dependencies: [status_bar_block_encoding_dependency, json_glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
