established_theme_reading_dependency = declare_dependency(
    link_with: static_library(
        'established-theme-reading',
        'established-theme-reading.c',
        dependencies: [compositor_bar_appearance_dependency, compositor_ipc_client_dependency, theme_palette_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [compositor_bar_appearance_dependency, theme_palette_dependency],
)

test(
    'established-theme-reading',
    executable(
        'test-established-theme-reading',
        'tests/test-established-theme-reading.c',
        dependencies: established_theme_reading_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
