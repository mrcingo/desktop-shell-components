compositor_bar_appearance_dependency = declare_dependency(
    link_with: static_library(
        'compositor-bar-appearance',
        'compositor-bar-appearance.c',
        dependencies: [
            compositor_ipc_client_dependency,
            typed_json_member_lookup_dependency,
            dependency('pango'),
        ],
    ),
    include_directories: include_directories('.'),
    dependencies: compositor_ipc_client_dependency,
)

test(
    'compositor-bar-appearance',
    executable(
        'test-compositor-bar-appearance',
        'tests/test-compositor-bar-appearance.c',
        dependencies: compositor_bar_appearance_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
