redundant_layout_wrapper_finder_dependency = declare_dependency(
    link_with: static_library(
        'redundant-layout-wrapper-finder',
        'redundant-layout-wrapper-finder.c',
        dependencies: [typed_json_member_lookup_dependency, json_glib_dependency],
    ),
    include_directories: include_directories('.'),
    dependencies: json_glib_dependency,
)

test(
    'redundant-layout-wrapper-finder',
    executable(
        'test-redundant-layout-wrapper-finder',
        'tests/test-redundant-layout-wrapper-finder.c',
        dependencies: redundant_layout_wrapper_finder_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
