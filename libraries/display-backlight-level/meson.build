display_backlight_level_dependency = declare_dependency(
    link_with: static_library(
        'display-backlight-level',
        'display-backlight-level.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [],
)

test(
    'display-backlight-level',
    executable(
        'test-display-backlight-level',
        'tests/test-display-backlight-level.c',
        dependencies: [display_backlight_level_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
