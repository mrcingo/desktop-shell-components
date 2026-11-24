login_form_widget_dependency = declare_dependency(
    link_with: static_library(
        'login-form-widget',
        'login-form-widget.c',
        dependencies: gtk_dependency,
    ),
    include_directories: include_directories('.'),
    dependencies: gtk_dependency,
)

executable(
    'login-form-widget-preview',
    'development-preview/login-form-widget-preview.c',
    dependencies: [
        layer_shell_window_dependency,
        login_form_widget_dependency,
        shared_style_sheets_dependency,
    ],
    build_by_default: false,
)
