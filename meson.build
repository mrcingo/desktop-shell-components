project(
    'desktop-shell-components',
    'c',
    version: '1.0.0',
    license: '0BSD',
    license_files: 'LICENSE',
    meson_version: '>=1.1.0',
    default_options: [
        'c_std=c11',
        'warning_level=2',
        'werror=true',
    ],
)

add_project_arguments(
    '-D_POSIX_C_SOURCE=200809L',
    '-Wno-unused-parameter',
    language: 'c',
)

fs = import('fs')

# gtk4-layer-shell must be linked before libwayland-client.
layer_shell_dependency = dependency('gtk4-layer-shell-0', version: '>=1.3.0')
gtk_dependency = dependency('gtk4', version: '>=4.16')
gtk_wayland_dependency = dependency('gtk4-wayland', version: '>=4.16')
glib_dependency = dependency('glib-2.0', version: '>=2.76')
gio_dependency = dependency('gio-2.0', version: '>=2.76')
gio_unix_dependency = dependency('gio-unix-2.0', version: '>=2.76')
json_glib_dependency = dependency('json-glib-1.0', version: '>=1.6')
wayland_client_dependency = dependency('wayland-client')

test_environment = ['G_DEBUG=fatal-warnings']

prefix = get_option('prefix')
installation_paths = configuration_data({
    'bindir': prefix / get_option('bindir'),
    'sysconfdir': prefix / get_option('sysconfdir'),
    'shared_data_directory': prefix / get_option('datadir') / meson.project_name(),
    'session_entries_directory': prefix / get_option('datadir') / 'wayland-sessions',
})
shared_data_directory = installation_paths.get('shared_data_directory')

scdoc_dependency = dependency('scdoc', native: true,
    required: get_option('manual-pages'))

subdir('development-tools')
