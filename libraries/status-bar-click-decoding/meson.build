status_bar_click_decoding_dependency = declare_dependency(
    link_with: static_library(
        'status-bar-click-decoding',
        'status-bar-click-decoding.c',
        dependencies: [typed_json_member_lookup_dependency, json_glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [glib_dependency],
)

test(
    'status-bar-click-decoding',
    executable(
        'test-status-bar-click-decoding',
        'tests/test-status-bar-click-decoding.c',
        dependencies: [status_bar_click_decoding_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
