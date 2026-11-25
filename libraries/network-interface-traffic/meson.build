network_interface_traffic_dependency = declare_dependency(
    link_with: static_library(
        'network-interface-traffic',
        'network-interface-traffic.c',
        dependencies: [glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: [],
)

test(
    'network-interface-traffic',
    executable(
        'test-network-interface-traffic',
        'tests/test-network-interface-traffic.c',
        dependencies: [network_interface_traffic_dependency, glib_dependency],
    ),
    protocol: 'tap',
    env: test_environment,
)
