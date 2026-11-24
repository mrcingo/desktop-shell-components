program_command_line_dependency = declare_dependency(
    link_with: static_library(
        'program-command-line',
        'program-command-line.c',
        dependencies: glib_dependency,
    ),
    include_directories: include_directories('.'),
)
