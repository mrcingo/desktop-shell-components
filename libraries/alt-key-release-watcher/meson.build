alt_key_release_watcher_dependency = declare_dependency(
    link_with: static_library(
        'alt-key-release-watcher',
        'alt-key-release-watcher.c',
        dependencies: gtk_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: gtk_dependency,
)
