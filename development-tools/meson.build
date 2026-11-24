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
