# Checks that run with "meson test", and tools for testing in a headless
# compositor, built only on request. Nothing here is installed.

python_program = find_program('python3')

test(
    'source-files-are-plain-ascii',
    python_program,
    args: [files('check-plain-ascii-text.py'), meson.project_source_root()],
)

test(
    'functions-are-small',
    python_program,
    args: [
        files('check-function-size.py'),
        meson.project_source_root() / 'libraries',
        meson.project_source_root() / 'programs',
        meson.project_source_root() / 'development-tools',
    ],
)

executable(
    'list-wayland-globals',
    'list-wayland-globals.c',
    dependencies: wayland_client_dependency,
    build_by_default: false,
)

executable(
    'send-virtual-key-presses',
    'send-virtual-key-presses.c',
    dependencies: [
        virtual_keyboard_unstable_v1_protocol_dependency,
        dependency('xkbcommon'),
        glib_dependency,
    ],
    build_by_default: false,
)

executable(
    'send-virtual-pointer-events',
    'send-virtual-pointer-events.c',
    dependencies: [
        wlr_virtual_pointer_unstable_v1_protocol_dependency,
        glib_dependency,
    ],
    build_by_default: false,
)

# Looks at the generated GTK 3 style sheet, when GTK 3 is installed.
toolkit_3_dependency = dependency('gtk+-3.0', required: false)

if toolkit_3_dependency.found()
    executable(
        'preview-toolkit-3-widgets',
        'preview-toolkit-3-widgets.c',
        dependencies: toolkit_3_dependency,
        build_by_default: false,
    )
endif
