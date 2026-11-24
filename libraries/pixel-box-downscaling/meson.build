pixel_box_downscaling_dependency = declare_dependency(
    link_with: static_library(
        'pixel-box-downscaling',
        'pixel-box-downscaling.c',
        dependencies: glib_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: glib_dependency,
)

test(
    'pixel-box-downscaling',
    executable(
        'test-pixel-box-downscaling',
        'tests/test-pixel-box-downscaling.c',
        dependencies: pixel_box_downscaling_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
