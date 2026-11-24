shared_style_sheets_dependency = declare_dependency(
    link_with: static_library(
        'shared-style-sheets',
        'shared-style-sheets.c',
        dependencies: gtk_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: gtk_dependency,
)
