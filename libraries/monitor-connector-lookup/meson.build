monitor_connector_lookup_dependency = declare_dependency(
    link_with: static_library(
        'monitor-connector-lookup',
        'monitor-connector-lookup.c',
        dependencies: gtk_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: gtk_dependency,
)
