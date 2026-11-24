executable(
    'desktop-shell-workspace-window-bar',
    [
        'workspace-window-bar-application.c',
        'workspace-window-bar-main.c',
        'workspace-window-bar-panel.c',
        'workspace-window-bar-settings.c',
    ],
    c_args: [
        '-DG_LOG_DOMAIN="workspace-window-bar"',
        '-DINSTALLED_DATA_DIRECTORY="@0@"'.format(shared_data_directory),
    ],
    dependencies: [
        compositor_matched_appearance_dependency,
        compositor_state_tracker_dependency,
        shared_style_sheets_dependency,
        layer_shell_window_dependency,
        per_monitor_objects_dependency,
        program_command_line_dependency,
        program_configuration_file_dependency,
        software_rendering_preference_dependency,
        single_instance_lock_dependency,
        window_label_format_dependency,
    ],
    install: true,
)

configure_file(
    input: 'workspace-window-bar-sway-configuration.conf.in',
    output: '60-desktop-shell-workspace-window-bar.conf',
    configuration: installation_paths,
    install_dir: get_option('sysconfdir') / 'sway' / 'config.d',
)

if scdoc_dependency.found()
    custom_target(
        'desktop-shell-workspace-window-bar.1',
        input: configure_file(
            input: 'desktop-shell-workspace-window-bar.1.scd.in',
            output: 'desktop-shell-workspace-window-bar.1.scd',
            configuration: installation_paths,
        ),
        output: 'desktop-shell-workspace-window-bar.1',
        command: scdoc_dependency.get_variable('scdoc'),
        feed: true,
        capture: true,
        install: true,
        install_dir: get_option('mandir') / 'man1',
    )
endif
