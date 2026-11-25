notification_sound_player_dependency = declare_dependency(
    link_with: static_library(
        'notification-sound-player',
        'notification-sound-player.c',
        dependencies: [gio_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [glib_dependency],
)

test(
    'notification-sound-player',
    executable(
        'test-notification-sound-player',
        'tests/test-notification-sound-player.c',
        dependencies: [notification_sound_player_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
