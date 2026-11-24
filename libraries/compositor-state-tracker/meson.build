compositor_state_tracker_dependency = declare_dependency(
    link_with: static_library(
        'compositor-state-tracker',
        'compositor-state-tracker.c',
        dependencies: [
            compositor_ipc_client_dependency,
            compositor_window_tree_dependency,
            typed_json_member_lookup_dependency,
            window_focus_history_dependency,
        ],
    ),
    include_directories: include_directories('.'),
    dependencies: [
        compositor_ipc_client_dependency,
        compositor_window_tree_dependency,
        gio_dependency,
        window_focus_history_dependency,
    ],
)

test(
    'compositor-state-tracker',
    executable(
        'test-compositor-state-tracker',
        'tests/test-compositor-state-tracker.c',
        dependencies: [
            compositor_state_tracker_dependency,
            compositor_ipc_message_format_dependency,
        ],
    ),
    protocol: 'tap',
    env: test_environment,
)
