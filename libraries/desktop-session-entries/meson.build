desktop_session_entries_dependency = declare_dependency(
    link_with: static_library(
        'desktop-session-entries',
        'desktop-session-entries.c',
        c_args: '-DG_LOG_DOMAIN="desktop-session-entries"',
        dependencies: glib_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: glib_dependency,
)

test(
    'desktop-session-entries',
    executable(
        'test-desktop-session-entries',
        'tests/test-desktop-session-entries.c',
        c_args: '-DG_LOG_DOMAIN="desktop-session-entries"',
        dependencies: desktop_session_entries_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
