# Client bindings for the Wayland protocols the libraries use, generated
# from the protocol descriptions installed by wayland-protocols. Each one
# becomes a dependency named after its protocol.

wayland_protocols_directory = dependency(
    'wayland-protocols',
    version: '>=1.40',
).get_variable('pkgdatadir')

wayland_scanner_program = find_program(
    dependency('wayland-scanner', native: true).get_variable('wayland_scanner'),
)

foreach protocol_name : [
    'ext-foreign-toplevel-list-v1',
    'ext-image-capture-source-v1',
    'ext-image-copy-capture-v1',
]
    protocol_family = protocol_name.substring(0, -3)
    protocol_description = (
        wayland_protocols_directory
        / 'staging'
        / protocol_family
        / protocol_name + '.xml'
    )

    client_header = custom_target(
        protocol_name + '-client-protocol.h',
        input: protocol_description,
        output: protocol_name + '-client-protocol.h',
        command: [wayland_scanner_program, 'client-header', '@INPUT@', '@OUTPUT@'],
    )

    interface_code = custom_target(
        protocol_name + '-protocol.c',
        input: protocol_description,
        output: protocol_name + '-protocol.c',
        command: [wayland_scanner_program, 'private-code', '@INPUT@', '@OUTPUT@'],
    )

    set_variable(
        protocol_name.underscorify() + '_protocol_dependency',
        declare_dependency(
            link_with: static_library(
                protocol_name + '-protocol',
                [interface_code, client_header],
                dependencies: wayland_client_dependency,
            ),
            sources: client_header,
            dependencies: wayland_client_dependency,
        ),
    )
endforeach

# Used only by the development tools, to type into a headless compositor.
virtual_keyboard_description = files('unstable/virtual-keyboard-unstable-v1.xml')

virtual_keyboard_unstable_v1_protocol_dependency = declare_dependency(
    link_with: static_library(
        'virtual-keyboard-unstable-v1-protocol',
        [
            custom_target(
                'virtual-keyboard-unstable-v1-protocol.c',
                input: virtual_keyboard_description,
                output: 'virtual-keyboard-unstable-v1-protocol.c',
                command: [wayland_scanner_program, 'private-code', '@INPUT@', '@OUTPUT@'],
            ),
        ],
        dependencies: wayland_client_dependency,
    ),
    sources: custom_target(
        'virtual-keyboard-unstable-v1-client-protocol.h',
        input: virtual_keyboard_description,
        output: 'virtual-keyboard-unstable-v1-client-protocol.h',
        command: [wayland_scanner_program, 'client-header', '@INPUT@', '@OUTPUT@'],
    ),
    dependencies: wayland_client_dependency,
)
