executable(
    'desktop-shell-window-snap',
    [
        'window-snap-application.c',
        'window-snap-main.c',
        'window-snap-overlay.c',
        'window-snap-settings.c',
    ],
    c_args: [
        '-DG_LOG_DOMAIN="window-snap"',
        '-DINSTALLED_DATA_DIRECTORY="@0@"'.format(shared_data_directory),
    ],
    dependencies: [
        compositor_matched_appearance_dependency,
        compositor_state_tracker_dependency,
        layer_shell_window_dependency,
        monitor_connector_lookup_dependency,
        program_command_line_dependency,
        program_configuration_file_dependency,
        replaced_executable_restart_dependency,
        shared_style_sheets_dependency,
        single_instance_lock_dependency,
        software_rendering_preference_dependency,
        window_snap_keyboard_steps_dependency,
        floating_window_arrangement_dependency,
        window_snap_placement_dependency,
        meson.get_compiler('c').find_library('m'),
    ],
    install: true,
)

configure_file(
    input: 'window-snap-sway-configuration.conf.in',
    output: '60-desktop-shell-window-snap.conf',
    configuration: installation_paths,
    install_dir: get_option('sysconfdir') / 'sway' / 'config.d',
)

if scdoc_dependency.found()
    custom_target(
        'desktop-shell-window-snap.1',
        input: configure_file(
            input: 'desktop-shell-window-snap.1.scd.in',
            output: 'desktop-shell-window-snap.1.scd',
            configuration: installation_paths,
        ),
        output: 'desktop-shell-window-snap.1',
        command: scdoc_dependency.get_variable('scdoc'),
        feed: true,
        capture: true,
        install: true,
        install_dir: get_option('mandir') / 'man1',
    )
endif
