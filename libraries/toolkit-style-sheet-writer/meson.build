toolkit_style_sheet_writer_dependency = declare_dependency(
    link_with: static_library(
        'toolkit-style-sheet-writer',
        'toolkit-style-sheet-writer.c',
        dependencies: theme_palette_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: theme_palette_dependency,
)

test(
    'toolkit-style-sheet-writer',
    executable(
        'test-toolkit-style-sheet-writer',
        'tests/test-toolkit-style-sheet-writer.c',
        dependencies: toolkit_style_sheet_writer_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)

# The generated sheets must parse in the toolkits that read them.
foreach toolkit_version : [['3', dependency('gtk+-3.0', required: false)], ['4', gtk_dependency]]
    if toolkit_version[1].found()
        test(
            'toolkit-style-sheet-parsing-' + toolkit_version[0],
            executable(
                'test-toolkit-style-sheet-parsing-' + toolkit_version[0],
                'tests/test-toolkit-style-sheet-parsing.c',
                c_args: '-DTOOLKIT_MAJOR_VERSION=' + toolkit_version[0],
                dependencies: [toolkit_style_sheet_writer_dependency, toolkit_version[1]],
            ),
            protocol: 'tap',
            env: test_environment,
        )
    endif
endforeach
