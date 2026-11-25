executable(
    'desktop-shell-notification-daemon',
    [
        'notification-daemon-application.c',
        'notification-daemon-main.c',
        'notification-daemon-settings.c',
    ],
    c_args: [
        '-DG_LOG_DOMAIN="notification-daemon"',
        '-DINSTALLED_DATA_DIRECTORY="@0@"'.format(shared_data_directory),
        '-DPROJECT_VERSION="@0@"'.format(meson.project_version()),
    ],
    dependencies: [
        compositor_ipc_client_dependency,
        json_glib_dependency,
        notification_rules_dependency,
        notification_sound_player_dependency,
        notification_specification_server_dependency,
        notification_store_dependency,
        process_window_focusing_dependency,
        program_command_line_dependency,
        program_configuration_file_dependency,
        replaced_executable_restart_dependency,
        single_instance_lock_dependency,
        typed_json_member_lookup_dependency,
    ],
    install: true,
)

configure_file(
    input: 'notification-daemon-sway-configuration.conf.in',
    output: '60-desktop-shell-notification-daemon.conf',
    configuration: installation_paths,
    install_dir: get_option('sysconfdir') / 'sway' / 'config.d',
)

if scdoc_dependency.found()
    custom_target(
        'desktop-shell-notification-daemon.1',
        input: configure_file(
            input: 'desktop-shell-notification-daemon.1.scd.in',
            output: 'desktop-shell-notification-daemon.1.scd',
            configuration: installation_paths,
        ),
        output: 'desktop-shell-notification-daemon.1',
        command: scdoc_dependency.get_variable('scdoc'),
        feed: true,
        capture: true,
        install: true,
        install_dir: get_option('mandir') / 'man1',
    )
endif
