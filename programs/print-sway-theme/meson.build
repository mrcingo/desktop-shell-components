executable(
    'desktop-shell-print-sway-theme',
    'print-sway-theme-main.c',
    dependencies: compositor_bar_appearance_dependency,
    install: true,
)

if scdoc_dependency.found()
    custom_target(
        'desktop-shell-print-sway-theme.1',
        input: configure_file(
            input: 'desktop-shell-print-sway-theme.1.scd.in',
            output: 'desktop-shell-print-sway-theme.1.scd',
            configuration: installation_paths,
        ),
        output: 'desktop-shell-print-sway-theme.1',
        command: scdoc_dependency.get_variable('scdoc'),
        feed: true,
        capture: true,
        install: true,
        install_dir: get_option('mandir') / 'man1',
    )
endif
