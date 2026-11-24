toplevel_window_capture_dependency = declare_dependency(
    link_with: static_library(
        'toplevel-window-capture',
        'toplevel-window-capture.c',
        c_args: '-DG_LOG_DOMAIN="toplevel-window-capture"',
        dependencies: [
            ext_image_capture_source_v1_protocol_dependency,
            ext_image_copy_capture_v1_protocol_dependency,
            foreign_toplevel_registry_dependency,
            pixel_box_downscaling_dependency,
            gtk_wayland_dependency,
            shared_memory_buffer_dependency,
            wayland_pixel_format_conversion_dependency,
        ],
    ),
    include_directories: include_directories('.'),
    dependencies: gtk_dependency,
)
