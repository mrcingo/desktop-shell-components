compositor_window_tree_dependency = declare_dependency(
    link_with: static_library(
        'compositor-window-tree',
        'compositor-window-tree.c',
        dependencies: typed_json_member_lookup_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: json_glib_dependency,
)

test(
    'compositor-window-tree',
    executable(
        'test-compositor-window-tree',
        'tests/test-compositor-window-tree.c',
        c_args: '-DEXAMPLE_WINDOW_TREE_PATH="@0@"'.format(
            meson.current_source_dir() / 'tests' / 'example-window-tree.json'),
        dependencies: compositor_window_tree_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
