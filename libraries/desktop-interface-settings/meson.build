desktop_interface_settings_dependency = declare_dependency(
    link_with: static_library(
        'desktop-interface-settings',
        'desktop-interface-settings.c',
        dependencies: gio_dependency,
    ),
    include_directories: include_directories('.'),
)

test(
    'desktop-interface-settings',
    executable(
        'test-desktop-interface-settings',
        'tests/test-desktop-interface-settings.c',
        dependencies: [desktop_interface_settings_dependency, gio_dependency],
    ),
    protocol: 'tap',
    env: test_environment + ['GSETTINGS_BACKEND=memory'],
)
