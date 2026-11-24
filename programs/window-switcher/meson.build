executable(
    'desktop-shell-window-switcher',
    [
        'window-switcher-application.c',
        'window-switcher-main.c',
        'window-switcher-overlay.c',
        'window-switcher-settings.c',
    ],
    c_args: [
        '-DG_LOG_DOMAIN="window-switcher"',
        '-DINSTALLED_DATA_DIRECTORY="@0@"'.format(shared_data_directory),
    ],
    dependencies: [
        alt_key_release_watcher_dependency,
        compositor_matched_appearance_dependency,
        compositor_state_tracker_dependency,
        layer_shell_window_dependency,
        monitor_connector_lookup_dependency,
        program_command_line_dependency,
        program_configuration_file_dependency,
        software_rendering_preference_dependency,
        shared_style_sheets_dependency,
        single_instance_lock_dependency,
        toplevel_window_capture_dependency,
        window_label_format_dependency,
        meson.get_compiler('c').find_library('m'),
    ],
    install: true,
)

configure_file(
    input: 'window-switcher-sway-configuration.conf.in',
    output: '60-desktop-shell-window-switcher.conf',
    configuration: installation_paths,
    install_dir: get_option('sysconfdir') / 'sway' / 'config.d',
)

if scdoc_dependency.found()
    custom_target(
        'desktop-shell-window-switcher.1',
        input: configure_file(
            input: 'desktop-shell-window-switcher.1.scd.in',
            output: 'desktop-shell-window-switcher.1.scd',
            configuration: installation_paths,
        ),
        output: 'desktop-shell-window-switcher.1',
        command: scdoc_dependency.get_variable('scdoc'),
        feed: true,
        capture: true,
        install: true,
        install_dir: get_option('mandir') / 'man1',
    )
endif
