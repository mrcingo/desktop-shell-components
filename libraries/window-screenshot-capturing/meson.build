window_screenshot_capturing_dependency = declare_dependency(
    link_with: static_library(
        'window-screenshot-capturing',
        'window-screenshot-capturing.c',
        c_args: '-DG_LOG_DOMAIN="window-screenshot-capturing"',
        dependencies: [
            exclusive_file_writing_dependency,
            toplevel_window_capture_dependency,
            window_screenshot_file_naming_dependency,
            gtk_dependency,
        ],
    ),
    include_directories: include_directories('.'),
    dependencies: gtk_dependency,
)
