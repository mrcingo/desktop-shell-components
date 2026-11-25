utility_program_style_writer_dependency = declare_dependency(
    link_with: static_library(
        'utility-program-style-writer',
        'utility-program-style-writer.c',
        dependencies: theme_palette_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: theme_palette_dependency,
)

test(
    'utility-program-style-writer',
    executable(
        'test-utility-program-style-writer',
        'tests/test-utility-program-style-writer.c',
        dependencies: utility_program_style_writer_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
