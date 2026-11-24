per_monitor_objects_dependency = declare_dependency(
    link_with: static_library(
        'per-monitor-objects',
        'per-monitor-objects.c',
        dependencies: gtk_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: gtk_dependency,
)
