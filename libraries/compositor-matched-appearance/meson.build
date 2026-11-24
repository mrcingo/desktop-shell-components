compositor_matched_appearance_dependency = declare_dependency(
    link_with: static_library(
        'compositor-matched-appearance',
        'compositor-matched-appearance.c',
        dependencies: [
            compositor_bar_appearance_dependency,
            shared_style_sheets_dependency,
        ],
    ),
    include_directories: include_directories('.'),
    dependencies: [compositor_ipc_client_dependency, gtk_dependency],
)
