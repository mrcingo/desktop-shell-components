typed_json_member_lookup_dependency = declare_dependency(
    link_with: static_library(
        'typed-json-member-lookup',
        'typed-json-member-lookup.c',
        dependencies: json_glib_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: json_glib_dependency,
)

test(
    'typed-json-member-lookup',
    executable(
        'test-typed-json-member-lookup',
        'tests/test-typed-json-member-lookup.c',
        dependencies: typed_json_member_lookup_dependency,
    ),
    protocol: 'tap',
    env: test_environment,
)
