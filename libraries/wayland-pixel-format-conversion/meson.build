wayland_pixel_format_conversion_dependency = declare_dependency(
    link_with: static_library(
        'wayland-pixel-format-conversion',
        'wayland-pixel-format-conversion.c',
        dependencies: [gtk_dependency, wayland_client_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [gtk_dependency, wayland_client_dependency],
)

test(
    'wayland-pixel-format-conversion',
    executable(
        'test-wayland-pixel-format-conversion',
        'tests/test-wayland-pixel-format-conversion.c',
        dependencies: wayland_pixel_format_conversion_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
