executable(
    'desktop-shell-system-status-line',
    [
        'system-status-line-application.c',
        'system-status-line-blocks.c',
        'system-status-line-main.c',
        'system-status-line-settings.c',
    ],
    c_args: '-DG_LOG_DOMAIN="system-status-line"',
    dependencies: [
        audio_volume_control_dependency,
        clock_boundary_timing_dependency,
        compositor_keyboard_layout_follower_dependency,
        human_readable_quantities_dependency,
        polled_system_readings_dependency,
        program_command_line_dependency,
        program_configuration_file_dependency,
        status_bar_block_encoding_dependency,
        status_bar_click_decoding_dependency,
        system_service_readings_dependency,
        text_placeholder_expansion_dependency,
        gio_unix_dependency,
    ],
    install: true,
)

if scdoc_dependency.found()
    custom_target(
        'desktop-shell-system-status-line.1',
        input: configure_file(
            input: 'desktop-shell-system-status-line.1.scd.in',
            output: 'desktop-shell-system-status-line.1.scd',
            configuration: installation_paths,
        ),
        output: 'desktop-shell-system-status-line.1',
        command: scdoc_dependency.get_variable('scdoc'),
        feed: true,
        capture: true,
        install: true,
        install_dir: get_option('mandir') / 'man1',
    )
endif
