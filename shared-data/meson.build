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
    'default-settings/system-status-line.ini',
    'default-settings/notification-daemon.ini',
    'default-settings/window-switcher.ini',
    'default-settings/application-style-export.ini',
    'default-settings/window-context-menu.ini',
    'default-settings/tray-click-window-focus.ini',
    'default-settings/window-title-format.ini',
    'default-settings/workspace-window-bar.ini',
    install_dir: get_option('sysconfdir') / 'xdg' / meson.project_name(),
)

# Made from sine waves at build time and dedicated to the public domain (CC0).
custom_target(
    'notification-sounds',
    output: ['notification-chime.wav', 'notification-critical-chime.wav'],
    command: [find_program('python3'), files('../development-tools/generate-notification-sounds.py'), '@OUTDIR@'],
    install: true,
    install_dir: shared_data_directory,
)

# A system policy changes behaviour for every user, so it is installed only on request.
if get_option('drive-mount-policy')
    install_data(
        'system-policies/50-desktop-shell-drive-mount-policy.rules',
        install_dir: get_option('datadir') / 'polkit-1' / 'rules.d',
    )
endif
