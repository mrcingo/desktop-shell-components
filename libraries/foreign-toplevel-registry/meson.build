foreign_toplevel_registry_dependency = declare_dependency(
    link_with: static_library(
        'foreign-toplevel-registry',
        'foreign-toplevel-registry.c',
        dependencies: [
            ext_foreign_toplevel_list_v1_protocol_dependency,
            glib_dependency,
        ],
    ),
    include_directories: include_directories('.'),
    dependencies: ext_foreign_toplevel_list_v1_protocol_dependency,
)
