executable(
    'desktop-shell-application-style-export',
    [
        'application-style-export-application.c',
        'application-style-export-main.c',
        'application-style-export-settings.c',
    ],
    c_args: '-DG_LOG_DOMAIN="application-style-export"',
    dependencies: [
        desktop_interface_settings_dependency,
        established_theme_reading_dependency,
        managed_file_writing_dependency,
        program_command_line_dependency,
        program_configuration_file_dependency,
        toolkit_style_sheet_writer_dependency,
        utility_program_style_writer_dependency,
    ],
    install: true,
)

configure_file(
    input: 'application-style-export-sway-configuration.conf.in',
    output: '60-desktop-shell-application-style-export.conf',
    configuration: installation_paths,
    install_dir: get_option('sysconfdir') / 'sway' / 'config.d',
)

if scdoc_dependency.found()
    custom_target(
        'desktop-shell-application-style-export.1',
        input: configure_file(
            input: 'desktop-shell-application-style-export.1.scd.in',
            output: 'desktop-shell-application-style-export.1.scd',
            configuration: installation_paths,
        ),
        output: 'desktop-shell-application-style-export.1',
        command: scdoc_dependency.get_variable('scdoc'),
        feed: true,
        capture: true,
        install: true,
        install_dir: get_option('mandir') / 'man1',
    )
endif
