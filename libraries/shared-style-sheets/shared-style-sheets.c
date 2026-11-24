#include <stdbool.h>

#include <gtk/gtk.h>

#include "shared-style-sheets.h"

static const char shared_theme_file_name[]  = "shared-theme.css";
static const char user_configuration_name[] = "desktop-shell-components";

enum {
    SHIPPED_THEME_PRIORITY     = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION,
    SHIPPED_COMPONENT_PRIORITY = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1,
    USER_THEME_PRIORITY        = GTK_STYLE_PROVIDER_PRIORITY_USER,
    USER_COMPONENT_PRIORITY    = GTK_STYLE_PROVIDER_PRIORITY_USER + 1,
};

const char *
shared_style_sheets_resolve_data_directory(
    const char *installed_data_directory
)
{
    const char *development_directory = g_getenv("DESKTOP_SHELL_COMPONENTS_DATA_DIRECTORY");

    if (development_directory != NULL) {
        return development_directory;
    }

    return installed_data_directory;
}

static void
report_style_sheet_error(
    GtkCssProvider *provider,
    GtkCssSection  *section,
    const GError   *error,
    void           *user_data
)
{
    char *location = gtk_css_section_to_string(section);

    g_warning("%s: %s", location, error->message);
    g_free(location);
}

static void
load_style_sheet(
    GdkDisplay *display,
    const char *style_sheet_path,
    guint       priority
)
{
    GtkCssProvider *provider = gtk_css_provider_new();

    g_signal_connect(provider, "parsing-error", G_CALLBACK(report_style_sheet_error), NULL);
    gtk_css_provider_load_from_path(provider, style_sheet_path);
    gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider), priority);
    g_object_unref(provider);
}

/* User style sheets are optional, so a missing one is not an error. */
static void
load_style_sheet_if_present(
    GdkDisplay *display,
    const char *style_sheet_path,
    guint       priority
)
{
    if (!g_file_test(style_sheet_path, G_FILE_TEST_IS_REGULAR)) {
        return;
    }

    load_style_sheet(display, style_sheet_path, priority);
}

/* Loads DIRECTORY/FILE_NAME, skipping it when it is optional and missing. */
static void
load_style_sheet_from_directory(
    GdkDisplay *display,
    const char *directory,
    const char *file_name,
    guint       priority,
    bool        is_optional
)
{
    char *style_sheet_path = g_build_filename(directory, file_name, NULL);

    if (is_optional) {
        load_style_sheet_if_present(display, style_sheet_path, priority);
    } else {
        load_style_sheet(display, style_sheet_path, priority);
    }

    g_free(style_sheet_path);
}

static void
load_shipped_style_sheets(
    GdkDisplay *display,
    const char *data_directory,
    const char *component_file_name,
    const char *style_sheet_override_path
)
{
    load_style_sheet_from_directory(display, data_directory, shared_theme_file_name, SHIPPED_THEME_PRIORITY, false);

    if (style_sheet_override_path != NULL) {
        load_style_sheet(display, style_sheet_override_path, SHIPPED_COMPONENT_PRIORITY);
        return;
    }

    load_style_sheet_from_directory(display, data_directory, component_file_name, SHIPPED_COMPONENT_PRIORITY, false);
}

static void
load_user_style_sheets(
    GdkDisplay *display,
    const char *component_file_name
)
{
    char *user_directory = g_build_filename(g_get_user_config_dir(), user_configuration_name, NULL);

    load_style_sheet_from_directory(display, user_directory, shared_theme_file_name, USER_THEME_PRIORITY, true);
    load_style_sheet_from_directory(display, user_directory, component_file_name, USER_COMPONENT_PRIORITY, true);
    g_free(user_directory);
}

void
shared_style_sheets_load_for_display(
    GdkDisplay *display,
    const char *data_directory,
    const char *component_name,
    const char *style_sheet_override_path
)
{
    char *component_file_name = g_strconcat(component_name, ".css", NULL);

    load_shipped_style_sheets(display, data_directory, component_file_name, style_sheet_override_path);
    load_user_style_sheets(display, component_file_name);
    g_free(component_file_name);
}
