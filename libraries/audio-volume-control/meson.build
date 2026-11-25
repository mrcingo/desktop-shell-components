audio_volume_control_dependency = declare_dependency(
    link_with: static_library(
        'audio-volume-control',
        'audio-volume-control.c',
        dependencies: [glib_dependency, dependency('libpulse'), dependency('libpulse-mainloop-glib')],
    ),
    include_directories: include_directories('.'),
    dependencies: [glib_dependency],
)

test(
    'audio-volume-control',
    executable(
        'test-audio-volume-control',
        'tests/test-audio-volume-control.c',
        dependencies: [audio_volume_control_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
