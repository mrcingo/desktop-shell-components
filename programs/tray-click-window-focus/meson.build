executable(
    'desktop-shell-tray-click-window-focus',
    [
        'tray-click-window-focus-application.c',
        'tray-click-window-focus-main.c',
        'tray-click-window-focus-settings.c',
    ],
    c_args: '-DG_LOG_DOMAIN="tray-click-window-focus"',
    dependencies: [
        compositor_ipc_client_dependency,
        json_glib_dependency,
        process_window_focusing_dependency,
        program_command_line_dependency,
        program_configuration_file_dependency,
        replaced_executable_restart_dependency,
        single_instance_lock_dependency,
        status_notifier_activation_watcher_dependency,
        typed_json_member_lookup_dependency,
    ],
    install: true,
)

configure_file(
    input: 'tray-click-window-focus-sway-configuration.conf.in',
    output: '60-desktop-shell-tray-click-window-focus.conf',
    configuration: installation_paths,
    install_dir: get_option('sysconfdir') / 'sway' / 'config.d',
)

if scdoc_dependency.found()
    custom_target(
        'desktop-shell-tray-click-window-focus.1',
        input: configure_file(
            input: 'desktop-shell-tray-click-window-focus.1.scd.in',
            output: 'desktop-shell-tray-click-window-focus.1.scd',
            configuration: installation_paths,
        ),
        output: 'desktop-shell-tray-click-window-focus.1',
        command: scdoc_dependency.get_variable('scdoc'),
        feed: true,
        capture: true,
        install: true,
        install_dir: get_option('mandir') / 'man1',
    )
endif
