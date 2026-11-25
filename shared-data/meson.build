install_data(
    'shared-theme.css',
    'login-greeter.css',
    'workspace-window-bar.css',
    'window-switcher.css',
    'window-context-menu.css',
    install_dir: shared_data_directory,
)

install_data(
    'default-settings/login-greeter.ini',
    'default-settings/window-switcher.ini',
    'default-settings/window-context-menu.ini',
    'default-settings/window-title-format.ini',
    'default-settings/workspace-window-bar.ini',
    install_dir: get_option('sysconfdir') / 'xdg' / meson.project_name(),
)
