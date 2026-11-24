layer_shell_window_dependency = declare_dependency(
    link_with: static_library(
        'layer-shell-window',
        'layer-shell-window.c',
        dependencies: [layer_shell_dependency, gtk_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [layer_shell_dependency, gtk_dependency],
)
