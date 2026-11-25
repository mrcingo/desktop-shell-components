compositor_attention_requests_dependency = declare_dependency(
    link_with: static_library(
        'compositor-attention-requests',
        'compositor-attention-requests.c',
        dependencies: [compositor_ipc_client_dependency, compositor_window_tree_dependency, typed_json_member_lookup_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [compositor_ipc_client_dependency],
)

test(
    'compositor-attention-requests',
    executable(
        'test-compositor-attention-requests',
        'tests/test-compositor-attention-requests.c',
        dependencies: [compositor_attention_requests_dependency, compositor_ipc_message_format_dependency, gio_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
