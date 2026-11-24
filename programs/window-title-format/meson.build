executable(
    'desktop-shell-window-title-format',
    [
        'window-title-format-application.c',
        'window-title-format-main.c',
        'window-title-format-settings.c',
    ],
    c_args: '-DG_LOG_DOMAIN="window-title-format"',
    dependencies: [
        compositor_state_tracker_dependency,
        compositor_title_bar_style_dependency,
        program_command_line_dependency,
        program_configuration_file_dependency,
        replaced_executable_restart_dependency,
        rendered_text_width_measurement_dependency,
        single_instance_lock_dependency,
        typed_json_member_lookup_dependency,
        window_title_fitting_dependency,
        window_title_format_command_dependency,
    ],
    install: true,
)

configure_file(
    input: 'window-title-format-sway-configuration.conf.in',
    output: '60-desktop-shell-window-title-format.conf',
    configuration: installation_paths,
    install_dir: get_option('sysconfdir') / 'sway' / 'config.d',
)

if scdoc_dependency.found()
    custom_target(
        'desktop-shell-window-title-format.1',
        input: configure_file(
            input: 'desktop-shell-window-title-format.1.scd.in',
            output: 'desktop-shell-window-title-format.1.scd',
            configuration: installation_paths,
        ),
        output: 'desktop-shell-window-title-format.1',
        command: scdoc_dependency.get_variable('scdoc'),
        feed: true,
        capture: true,
        install: true,
        install_dir: get_option('mandir') / 'man1',
    )
endif
