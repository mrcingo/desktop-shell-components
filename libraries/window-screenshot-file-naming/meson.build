window_screenshot_file_naming_dependency = declare_dependency(
    link_with: static_library(
        'window-screenshot-file-naming',
        'window-screenshot-file-naming.c',
        dependencies: glib_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: glib_dependency,
)

test(
    'window-screenshot-file-naming',
    executable(
        'test-window-screenshot-file-naming',
        'tests/test-window-screenshot-file-naming.c',
        dependencies: window_screenshot_file_naming_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
