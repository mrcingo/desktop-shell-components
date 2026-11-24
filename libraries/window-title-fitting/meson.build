window_title_fitting_dependency = declare_dependency(
    link_with: static_library(
        'window-title-fitting',
        'window-title-fitting.c',
        dependencies: [
            rendered_text_width_measurement_dependency,
            text_ellipsis_truncation_dependency,
            window_title_format_command_dependency,
            glib_dependency,
        ],
    ),
    include_directories: include_directories('.'),
    dependencies: [compositor_window_tree_dependency, rendered_text_width_measurement_dependency],
)

test(
    'window-title-fitting',
    executable(
        'test-window-title-fitting',
        'tests/test-window-title-fitting.c',
        dependencies: [window_title_fitting_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
