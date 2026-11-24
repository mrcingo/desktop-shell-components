executable(
    'desktop-shell-login-greeter',
    [
        'login-greeter-application.c',
        'login-greeter-keyboard-layout.c',
        'login-greeter-main.c',
        'login-greeter-settings.c',
    ],
    c_args: [
        '-DG_LOG_DOMAIN="login-greeter"',
        '-DINSTALLED_DATA_DIRECTORY="@0@"'.format(shared_data_directory),
        '-DSESSION_ENTRIES_DIRECTORY="@0@"'.format(installation_paths.get('session_entries_directory')),
    ],
    dependencies: [
        compositor_ipc_client_dependency,
        desktop_session_entries_dependency,
        program_command_line_dependency,
        program_configuration_file_dependency,
        software_rendering_preference_dependency,
        layer_shell_window_dependency,
        login_daemon_conversation_dependency,
        login_form_widget_dependency,
        per_monitor_objects_dependency,
        shared_style_sheets_dependency,
        system_keyboard_layout_dependency,
        typed_json_member_lookup_dependency,
    ],
    install: true,
)

configure_file(
    input: 'login-greeter-compositor-configuration.in',
    output: 'login-greeter-compositor-configuration',
    configuration: installation_paths,
    install_dir: get_option('sysconfdir') / 'greetd',
)

if scdoc_dependency.found()
    custom_target(
        'desktop-shell-login-greeter.1',
        input: configure_file(
            input: 'desktop-shell-login-greeter.1.scd.in',
            output: 'desktop-shell-login-greeter.1.scd',
            configuration: installation_paths,
        ),
        output: 'desktop-shell-login-greeter.1',
        command: scdoc_dependency.get_variable('scdoc'),
        feed: true,
        capture: true,
        install: true,
        install_dir: get_option('mandir') / 'man1',
    )
endif
